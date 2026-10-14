// SPDX-License-Identifier: MIT
#include "spectrum.h"
#include "../../common/fft.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

#include <fftw3.h>

namespace {

constexpr double kFullScale = 8192;    // 14-bit two's complement components
constexpr uint64_t kHorizon = 2048;    // slices: a slice still missing this far back is lost
// Slices that may still arrive from before a reported tuning change: reordering across the
// receive threads, and the unit's export lag behind its DMA ring.
constexpr uint64_t kSettleSlices = 1024;
constexpr size_t kHistory = 256;       // segments kept for tuning_over()

// Cosine-sum windows: w[n] = sum_k (-1)^k a_k cos(2 pi k n / N), periodic.
const std::vector<std::pair<std::string, std::vector<double>>> &window_table()
{
    static const std::vector<std::pair<std::string, std::vector<double>>> table = {
        {"rectangular", {1}},
        {"hann", {0.5, 0.5}},
        {"hamming", {0.54, 0.46}},
        {"blackman", {0.42, 0.5, 0.08}},
        {"blackman-harris-4", {0.35875, 0.48829, 0.14128, 0.01168}},
        {"blackman-harris-7",
         {0.27105140069342, 0.43329793923448, 0.21812299954311, 0.06592544638803, 0.01081174209837,
          0.00077658482522, 0.00001388721735}},
        {"flat-top", {0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368}},
    };
    return table;
}

std::mutex planner;  // FFTW's planner is not thread-safe; executing a plan is

// A sample word: I in [29:16], Q in [13:0], 14-bit two's complement.


}  // namespace

const std::vector<std::string> &spectrum_windows()
{
    static const std::vector<std::string> names = [] {
        std::vector<std::string> list;
        for (const auto &entry : window_table()) list.push_back(entry.first);
        return list;
    }();
    return names;
}

double spectrum_window_bandwidth(const std::string &window)
{
    const unsigned n = 4096;
    double sum = 0, squares = 0;
    for (const auto &entry : window_table()) {
        if (entry.first != window) continue;
        for (unsigned i = 0; i < n; ++i) {
            double w = 0, sign = 1;
            for (size_t k = 0; k < entry.second.size(); ++k, sign = -sign)
                w += sign * entry.second[k] * std::cos(2 * M_PI * double(k) * i / n);
            sum += w;
            squares += w * w;
        }
    }
    return sum ? n * squares / (sum * sum) : 1;
}

void SpectrumSettings::validate() const
{
    if (size < 512 || size > 262144 || (size & (size - 1)))
        throw std::runtime_error("FFT size must be a power of two from 512 to 262144");
    const auto &names = spectrum_windows();
    if (std::find(names.begin(), names.end(), window) == names.end())
        throw std::runtime_error("unknown FFT window " + window);
    if (!(rate >= 1 && rate <= 100)) throw std::runtime_error("FFT rate must be 1..100 frames per second");
    if (averaging > 100000) throw std::runtime_error("averaging must be at most 100000 blocks");
}

// One FFT size and window, shared by the workers: each runs the plan on its own arrays.
struct SpectrumEngine::Transform {
    unsigned size;
    std::vector<float> window;
    double scale;              // |X|^2 of a full-scale complex tone -> 1
    fftwf_plan plan;

    Transform(unsigned n, const std::string &name) : size(n), window(n)
    {
        const std::vector<double> *terms = nullptr;
        for (const auto &entry : window_table())
            if (entry.first == name) terms = &entry.second;
        double sum = 0;
        for (unsigned i = 0; i < n; ++i) {
            double w = 0, sign = 1;
            for (size_t k = 0; k < terms->size(); ++k, sign = -sign)
                w += sign * (*terms)[k] * std::cos(2 * M_PI * double(k) * i / n);
            window[i] = float(w);
            sum += w;
        }
        scale = 1 / ((kFullScale * sum) * (kFullScale * sum));
        std::lock_guard<std::mutex> lock(planner);
        fftwf_complex *in = fftwf_alloc_complex(n), *out = fftwf_alloc_complex(n);
        // Measuring finds a faster plan; the time limit keeps a new size from stalling the
        // stream for long (FFTW remembers plans it has made).
        fftwf_set_timelimit(1.0);
        plan = fftwf_plan_dft_1d(int(n), in, out, FFTW_FORWARD, w1700k_fft_plan_flags());
        fftwf_free(in);
        fftwf_free(out);
    }
    ~Transform()
    {
        std::lock_guard<std::mutex> lock(planner);
        fftwf_destroy_plan(plan);
    }
};

struct SpectrumEngine::Worker {
    std::thread thread;
    unsigned size = 0;
    fftwf_complex *in = nullptr, *out = nullptr;
    std::vector<float> power;
    ~Worker()
    {
        fftwf_free(in);
        fftwf_free(out);
    }
};

SpectrumEngine::SpectrumEngine(const StreamClient &stream, const Tuning &tuning, unsigned workers, Output output, const std::string &dsp_input)
    : dsp_input_(dsp_input), stream_(stream), output_(std::move(output))
{
    latest_.tuning = tuning;
    history_.push_back(latest_);
    if (!dsp_input_.empty()) {
        settings_.size = 2048;
        settings_.window = "hann";
        settings_.rate = 20;
        settings_.averaging = 1;
        settings_.peak = false;
        settings_.remove_dc = true;
        scheduler_ = std::thread([this] { read_dsp(); });
        return;  // No FFTW planning, CPU FFT workers, or raw IQ ingestion.
    }
    stats_.workers = std::max(1u, workers);
    transform(settings_.size, settings_.window);  // planned now, not while the stream runs past
    for (unsigned i = 0; i < stats_.workers; ++i) workers_.push_back(std::make_unique<Worker>());
    for (auto &w : workers_) w->thread = std::thread([this, p = w.get()] { work(*p); });
    scheduler_ = std::thread([this] { schedule(); });
}

SpectrumEngine::~SpectrumEngine()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    work_ready_.notify_all();
    scheduler_.join();
    for (auto &w : workers_) w->thread.join();
}

void SpectrumEngine::configure(const SpectrumSettings &settings)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!dsp_input_.empty() && (settings.size != 2048 || settings.window != "hann" ||
        settings.rate != 20 || settings.averaging != 1 || settings.peak || !settings.remove_dc))
        throw std::runtime_error("DSP0 preview uses 2048 points, Hann, DC removal and a 20 fps display");
    settings_ = settings;
    ++settings_version_;
}

SpectrumSettings SpectrumEngine::settings() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_;
}

SpectrumEngine::Stats SpectrumEngine::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

namespace {

// Is the segment at (run, from) past slot `index` of run `run_now`?
bool after(uint64_t run, uint64_t from, uint64_t run_now, uint64_t index)
{
    return run != run_now ? run > run_now : from > index;
}

}  // namespace

void SpectrumEngine::add_segment(Segment s)
{
    // Positions only move forward, even when the newest slice has not since the last change.
    if (s.run < latest_.run || (s.run == latest_.run && s.from < latest_.from)) {
        s.run = latest_.run;
        s.from = latest_.from;
    }
    latest_ = s;
    pending_.push_back(s);
    history_.push_back(s);
    while (history_.size() > kHistory) history_.pop_front();
}

void SpectrumEngine::set_tuning(const Tuning &tuning)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (latest_.valid && latest_.tuning == tuning) return;
    const StreamClient::Position p = stream_.position();
    add_segment({p.run, p.newest + kSettleSlices, true, tuning});
}

void SpectrumEngine::begin_change()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!latest_.valid) return;
    const StreamClient::Position p = stream_.position();
    add_segment({p.run, p.newest, false, latest_.tuning});
}

Tuning SpectrumEngine::tuning() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_.tuning;
}

std::optional<Tuning> SpectrumEngine::tuning_over(uint64_t run, uint64_t first, uint64_t last) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Segment *in_effect = nullptr;
    for (const Segment &s : history_) {
        if (!after(s.run, s.from, run, first)) in_effect = &s;
        else if (!after(s.run, s.from, run, last)) return std::nullopt;  // a change inside the range
    }
    if (!in_effect || !in_effect->valid) return std::nullopt;
    return in_effect->tuning;
}

std::shared_ptr<const SpectrumEngine::Transform> SpectrumEngine::transform(unsigned size, const std::string &window)
{
    std::lock_guard<std::mutex> lock(transforms_mutex_);
    auto &t = transforms_[{size, window}];
    if (!t) t = std::make_shared<const Transform>(size, window);
    return t;
}

// Moves the frontier over the ring (every slot before it holds its slice or an event, or was
// given up as lost) and hands each whole frame behind it to a worker. Frames are periods of
// sample time; their blocks are spread evenly over the period (overlapping when there are more
// than fit). A frame that comes due while every worker is busy and the queue is full is skipped,
// not waited for. Each frame carries the tuning in effect over all of it; one that spans a
// change, or falls in a change in progress, is dropped.
void SpectrumEngine::schedule()
{
    uint64_t run = 0, frontier = 0, next = 0, sequence = 0, applied = 0;  // next: first sample of the next frame
    uint64_t frame_pairs = 1, stride = 0;
    unsigned blocks = 1;
    bool replan = true;
    SpectrumSettings active;
    std::shared_ptr<const Transform> t;
    Segment current;
    std::deque<Segment> upcoming;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current = history_.front();
    }
    for (;;) {
        SpectrumSettings s;
        uint64_t version;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            s = settings_;
            version = settings_version_;
            upcoming.insert(upcoming.end(), pending_.begin(), pending_.end());
            pending_.clear();
        }
        const std::shared_ptr<const StreamView> view = stream_.view();
        const StreamClient::Position position = stream_.position();
        if (!view) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        const uint64_t r = position.run, newest = position.newest;
        if (r != run) {
            run = r;
            frontier = next = 0;
        }
        if (version != applied || !t) {
            if (!t || t->size != s.size || s.window != active.window) t = transform(s.size, s.window);
            active = s;
            applied = version;
            replan = true;
            next = std::max(next, frontier * kSliceSamples);
        }

        uint64_t lost = 0, skipped = 0, dropped = 0, retuned = 0;
        const uint64_t capacity = view->capacity();
        if (newest > frontier + capacity / 2) {  // far behind (planning a new size): what was there is gone
            skipped += newest - capacity / 4 - frontier;
            frontier = newest - capacity / 4;
            next = std::max(next, frontier * kSliceSamples);
        }
        while (frontier < newest) {
            const uint64_t tag = view->tag(frontier);
            if (StreamView::holds_samples(tag, run, frontier) || StreamView::holds_event(tag, run, frontier)) {
                ++frontier;
            } else if (StreamView::holds_loss(tag, run, frontier) || newest > frontier + kHorizon) {
                ++frontier;
                ++lost;
            } else {
                break;
            }
        }
        bool queued = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (;;) {
                while (!upcoming.empty() && !after(upcoming.front().run, upcoming.front().from, run, next / kSliceSamples)) {
                    current = upcoming.front();
                    upcoming.pop_front();
                    replan = true;
                }
                if (replan) {
                    const double rate = current.tuning.sample_rate;
                    frame_pairs = std::max<uint64_t>(active.size, uint64_t(std::llround(rate / active.rate)));
                    // "All": enough blocks, overlapping a little, that every sample is in one.
                    blocks = active.averaging ? active.averaging : unsigned((frame_pairs + active.size - 1) / active.size);
                    stride = blocks > 1 ? (frame_pairs - active.size) / (blocks - 1) : 0;
                    replan = false;
                }
                const uint64_t end = next + frame_pairs;
                if (end > frontier * kSliceSamples) break;
                if (!upcoming.empty() &&
                    !after(upcoming.front().run, upcoming.front().from, run, (end - 1) / kSliceSamples)) {
                    ++retuned;  // spans a change: resume where it takes effect
                    next = upcoming.front().from * kSliceSamples;
                    continue;
                }
                if (!current.valid) {
                    ++retuned;
                } else if (jobs_.size() < 2 * workers_.size()) {
                    Job job;
                    job.view = view;
                    job.sequence = sequence++;
                    job.run = run;
                    job.first = next;
                    job.stride = stride;
                    job.blocks = blocks;
                    job.transform = t;
                    job.tuning = current.tuning;
                    job.peak = active.peak;
                    job.remove_dc = active.remove_dc;
                    jobs_.push_back(std::move(job));
                    queued = true;
                } else {
                    dropped += frame_pairs;
                }
                next = end;
            }
            stats_.slices_lost += lost;
            stats_.slices_skipped += skipped;
            stats_.pairs_dropped += dropped;
            stats_.frames_retuned += retuned;
            stats_.backlog = newest - frontier;
            stats_.queued = unsigned(jobs_.size());
        }
        if (queued) work_ready_.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void SpectrumEngine::work(Worker &w)
{
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_ready_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
            if (stopping_) return;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        const Transform &t = *job.transform;
        const unsigned n = t.size;
        if (w.size != n) {
            fftwf_free(w.in);
            fftwf_free(w.out);
            w.in = fftwf_alloc_complex(n);
            w.out = fftwf_alloc_complex(n);
            w.power.assign(n, 0.0f);
            w.size = n;
        }
        std::fill(w.power.begin(), w.power.end(), 0.0f);

        unsigned used = 0;
        uint64_t missing = 0;
        for (unsigned b = 0; b < job.blocks; ++b) {
            const uint64_t position = job.first + b * job.stride;
            const uint64_t s0 = position / kSliceSamples, s1 = (position + n - 1) / kSliceSamples;
            auto present = [&] {
                for (uint64_t s = s0; s <= s1; ++s)
                    if (!StreamView::holds_samples(job.view->tag(s), job.run, s)) return false;
                return true;
            };
            if (!present()) {
                missing += n;
                continue;
            }
            // The block's samples, slice by slice: (fn)(sample number in block, I, Q).
            auto each = [&](auto fn) {
                uint64_t p = position;
                for (unsigned k = 0; k < n;) {
                    const int16_t *iq = job.view->slice(p / kSliceSamples);
                    unsigned offset = unsigned(p % kSliceSamples), m = std::min(n - k, kSliceSamples - offset);
                    for (unsigned j = 0; j < m; ++j) fn(k + j, float(iq[2 * (offset + j)]), float(iq[2 * (offset + j) + 1]));
                    k += m;
                    p += m;
                }
            };
            float mean_i = 0, mean_q = 0;
            if (job.remove_dc) {
                double sum_i = 0, sum_q = 0;
                each([&](unsigned, float i, float q) {
                    sum_i += i;
                    sum_q += q;
                });
                mean_i = float(sum_i / n);
                mean_q = float(sum_q / n);
            }
            const float *window = t.window.data();
            fftwf_complex *in = w.in;
            each([&](unsigned k, float i, float q) {
                in[k][0] = (i - mean_i) * window[k];
                in[k][1] = (q - mean_q) * window[k];
            });
            std::atomic_thread_fence(std::memory_order_acquire);
            if (!present()) {  // a slot was taken over while we read it
                missing += n;
                continue;
            }
            fftwf_execute_dft(t.plan, w.in, w.out);
            const fftwf_complex *out = w.out;
            float *power = w.power.data();
            if (job.peak) {
                for (unsigned k = 0; k < n; ++k)
                    power[k] = std::max(power[k], out[k][0] * out[k][0] + out[k][1] * out[k][1]);
            } else {
                for (unsigned k = 0; k < n; ++k) power[k] += out[k][0] * out[k][0] + out[k][1] * out[k][1];
            }
            ++used;
        }

        std::shared_ptr<SpectrumFrame> frame;
        if (used) {
            frame = std::make_shared<SpectrumFrame>();
            frame->run = job.run;
            frame->index = job.first;
            frame->center_hz = job.tuning.center_hz;
            frame->sample_rate = job.tuning.sample_rate;
            frame->blocks = used;
            frame->peak = job.peak;
            frame->db.resize(n);
            // I + jQ is RF minus LO (a carrier below the LO has a negative frequency): display
            // bin i is RF offset (i - n/2) * rate / n, FFT bin (i - n/2) mod n.
            const double scale = t.scale / (job.peak ? 1 : used);
            const float *power = w.power.data();
            for (unsigned i = 0; i < n; ++i)
                frame->db[i] = float(10 * std::log10(power[(i + n / 2) & (n - 1)] * scale + 1e-20));
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.blocks += used;
            stats_.pairs_used += uint64_t(used) * n;
            stats_.pairs_missing += missing;
            if (frame) ++stats_.frames;
        }
        finish(job.sequence, std::move(frame));
    }
}

// Results go out in the order the frames were scheduled; a frame with no usable block leaves
// no gap.
void SpectrumEngine::finish(uint64_t sequence, std::shared_ptr<SpectrumFrame> frame)
{
    std::lock_guard<std::mutex> lock(results_mutex_);
    results_[sequence] = std::move(frame);
    for (auto it = results_.find(emit_next_); it != results_.end(); it = results_.find(emit_next_)) {
        std::shared_ptr<SpectrumFrame> f = std::move(it->second);
        results_.erase(it);
        ++emit_next_;
        if (f) output_(std::move(f));
    }
}

// Atomic latest-frame file produced by tools/dsp/live.py. Wire format is explicitly
// little-endian: 56-byte header, then 2048 normalized linear-power float32 bins.
void SpectrumEngine::read_dsp()
{
    uint64_t last_time = 0, last_index = 0;
    for (;;) {
        const auto next_display = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
        }
        std::ifstream input(dsp_input_, std::ios::binary);
        std::vector<unsigned char> bytes(56 + 2048 * 4);
        if (input.read(reinterpret_cast<char *>(bytes.data()), bytes.size()) && input.peek() == EOF &&
            (std::memcmp(bytes.data(), "DSPFFT1\0", 8) == 0 || std::memcmp(bytes.data(), "DSPFFT2\0", 8) == 0)) {
            auto u32 = [&](size_t p) { uint32_t v=0; for(unsigned j=0;j<4;j++) v |= uint32_t(bytes[p+j]) << (8*j); return v; };
            auto u64 = [&](size_t p) { return uint64_t(u32(p)) | (uint64_t(u32(p+4)) << 32); };
            auto real = [&](size_t p) { uint64_t bits=u64(p); double v; std::memcpy(&v,&bits,8); return v; };
            const uint64_t time = u64(48);
            const double wall = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            const double center = real(24), rate = real(32);
            const uint32_t core_mask=bytes[6]=='2' ? u32(12) : 1;
            if (u32(8)==2048 && (bytes[6]=='2' ? core_mask>0 && core_mask<=7 : u32(12)==1) && time!=last_time &&
                wall - double(time)/1e9 >= -1 && wall - double(time)/1e9 < 1 &&
                std::isfinite(center) && center>0 && rate==40e6) {
                auto frame=std::make_shared<SpectrumFrame>();
                frame->run=1; frame->index=u64(16); frame->blocks=1;
                frame->center_hz=center; frame->sample_rate=rate; frame->db.resize(2048);
                bool valid=true;
                for(unsigned i=0;i<2048;i++) {
                    uint32_t bits=u32(56+4*((i+1024)%2048)); float power;
                    std::memcpy(&power,&bits,4);
                    if(!std::isfinite(power) || power<0) { valid=false; break; }
                    frame->db[i]=float(10*std::log10(double(power)+1e-20));
                }
                if(valid) {
                    last_time=time;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        // The producer runs independently of display pacing. Its
                        // sequence counts completed FFTs, including those between
                        // displayed snapshots; it does not prove continuous IQ.
                        const uint64_t blocks = last_index && frame->index > last_index
                                                ? frame->index-last_index : 1;
                        ++stats_.frames; stats_.blocks+=blocks; stats_.pairs_used+=2048*blocks;
                        stats_.last_frame=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        stats_.dsp_cycles=u32(40); stats_.dma_cycles=u32(44); stats_.dsp_core_mask=core_mask;
                        latest_.tuning={center,rate};
                    }
                    last_index=frame->index;
                    output_(frame);
                }
            }
        }
        // Presentation cadence must not sleep the DSP capture/processing loop.
        std::this_thread::sleep_until(next_display);
    }
}
