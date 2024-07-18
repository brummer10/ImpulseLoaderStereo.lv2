/*
 * Copyright (C) 2012 Hermann Meyer, Andreas Degert, Pete Shorthose, Steve Poskitt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * --------------------------------------------------------------------------
 */


#pragma once

#ifndef SRC_HEADERS_GX_CONVOLVER_H_
#define SRC_HEADERS_GX_CONVOLVER_H_

#include "zita-convolver.h"
#include "TwoStageFFTConvolver.h"
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "gx_resampler.h"

#include <sndfile.hh>

/* GxConvolver */

class Audiofile {
public:

    enum {
        TYPE_OTHER,
        TYPE_CAF,
        TYPE_WAV,
        TYPE_AIFF,
        TYPE_AMB
    };

    enum {
        FORM_OTHER,
        FORM_16BIT,
        FORM_24BIT,
        FORM_32BIT,
        FORM_FLOAT
    };

    enum {
        ERR_NONE    = 0,
        ERR_MODE    = -1,
        ERR_TYPE    = -2,
        ERR_FORM    = -3,
        ERR_OPEN    = -4,
        ERR_SEEK    = -5,
        ERR_DATA    = -6,
        ERR_READ    = -7,
        ERR_WRITE   = -8
    };

    Audiofile(void);
    ~Audiofile(void);

    int type(void) const      { return _type; }
    int form(void) const      { return _form; }
    int rate(void) const      { return _rate; }
    int chan(void) const      { return _chan; }
    unsigned int size(void) const { return _size; }

    int open_read(std::string name);
    int close(void);

    int seek(unsigned int posit);
    int read(float *data, unsigned int frames);

private:

    void reset(void);

    SNDFILE     *_sndfile;
    int          _type;
    int          _form;
    int          _rate;
    int          _chan;
    unsigned int _size;
};

class ConvolverBase
{
public:
    virtual bool start(int32_t policy, int32_t priority) { return false;};

    virtual bool configure(std::string fname, float gain, unsigned int delay, unsigned int offset,
                    unsigned int length, unsigned int size, unsigned int bufsize) { return false;};

    virtual bool compute(int32_t count, float* input, float *output) { return false;};

    virtual bool configure(std::string fname, float gain, float lgain,
        unsigned int delay, unsigned int ldelay, unsigned int offset,
        unsigned int length, unsigned int size, unsigned int bufsize) { return false;};

    virtual bool compute(int count, float* input1, float *input2,
                        float *output1, float *output2) { return false;};

    virtual bool checkstate() { return false;};
    virtual inline void set_not_runnable() {};
    virtual inline bool is_runnable() { return false;};
    virtual inline void set_buffersize(uint32_t sz) {};
    virtual inline void set_samplerate(uint32_t sr) {};
    virtual int stop_process() { return 0;};
    virtual int cleanup() { return 0;};
    ConvolverBase () {}
    virtual ~ConvolverBase () {}
};

class GxConvolverBase: public ConvolverBase, protected Convproc
{
protected:
  volatile bool ready;
  bool sync;
  void adjust_values(uint32_t audio_size, uint32_t& count, uint32_t& offset,
                     uint32_t& delay, uint32_t& ldelay, uint32_t& length,
                     uint32_t& size, uint32_t& bufsize);
  uint32_t buffersize;
  uint32_t samplerate;
  GxConvolverBase(): ready(false), sync(false), buffersize(), samplerate() {}
  ~GxConvolverBase();
public:
  inline void set_buffersize(uint32_t sz)
  {
    buffersize = sz;
  }
  inline uint32_t get_buffersize()
  {
    return buffersize;
  }
  inline void set_samplerate(uint32_t sr)
  {
    samplerate = sr;
  }
  inline uint32_t get_samplerate()
  {
    return samplerate;
  }
  bool checkstate();
  using Convproc::state;
  inline void set_not_runnable()
  {
    ready = false;
  }
  inline bool is_runnable()
  {
    return ready;
  }
  bool start(int32_t policy, int32_t priority);
  int stop_process() { return Convproc::stop_process();}
  int cleanup() { return Convproc::cleanup();}
  inline void set_sync(bool val)
  {
    sync = val;
  }
};

class GxConvolver: public GxConvolverBase {
private:
    gx_resample::StreamingResampler resamp;
    bool read_sndfile(Audiofile& audio, int nchan, int samplerate, const float *gain,
		      unsigned int *delay, unsigned int offset, unsigned int length);
public:
    GxConvolver()
      : GxConvolverBase(), resamp() {}
    bool configure(std::string fname, float gain, float lgain,
        unsigned int delay, unsigned int ldelay, unsigned int offset,
        unsigned int length, unsigned int size, unsigned int bufsize);
    bool compute(int count, float* input1, float *input2, float *output1, float *output2);
    bool configure(std::string fname, float gain, unsigned int delay, unsigned int offset,
		   unsigned int length, unsigned int size, unsigned int bufsize);
    bool compute(int count, float* input, float *output);
};


class DoubleThreadConvolver;

class ConvolverWorker {
private:
    std::atomic<bool> _execute;
    std::thread _thd;
    std::mutex m;
    void set_priority();
    void run();
    DoubleThreadConvolver &_xr;

public:
    ConvolverWorker(DoubleThreadConvolver &xr);
    ~ConvolverWorker();
    void stop();
    void start();
    bool is_running() const noexcept;
    std::condition_variable cv;
};

class DoubleThreadConvolver: public ConvolverBase, public fftconvolver::TwoStageFFTConvolver
{
public:
    std::mutex mo;
    std::condition_variable co;
    bool start(int32_t policy, int32_t priority) { 
        work.start(); 
        return ready;}

    bool configure(std::string fname, float gain, unsigned int delay, unsigned int offset,
                    unsigned int length, unsigned int size, unsigned int bufsize);

    bool compute(int32_t count, float* input, float *output);

    bool configure(std::string fname, float gain, float lgain,
        unsigned int delay, unsigned int ldelay, unsigned int offset,
        unsigned int length, unsigned int size, unsigned int bufsize);

    bool compute(int count, float* input1, float *input2, float *output1, float *output2);

    bool checkstate() { return true;}

    inline void set_not_runnable() { ready = false;}

    inline bool is_runnable() { return ready;}

    inline void set_buffersize(uint32_t sz) { buffersize = sz;}

    inline void set_samplerate(uint32_t sr) { samplerate = sr;}

    int stop_process() {
            reset();
            ready = false;
            return 0;}

    int cleanup () {
            reset();
            return 0;}

    DoubleThreadConvolver()
        : resamp(), ready(false), samplerate(0), work(*this) {
            timeoutPeriod = std::chrono::microseconds(2000);
            setWait.store(false, std::memory_order_release);
            conv2 = nullptr; channel = 0;}

    ~DoubleThreadConvolver() { reset(); work.stop(); delete conv2;}

protected:
    virtual void startBackgroundProcessing();
    virtual void waitForBackgroundProcessing();

private:
    friend class ConvolverWorker;
    gx_resample::BufferResampler resamp;
    volatile bool ready;
    uint32_t buffersize;
    uint32_t samplerate;
    uint32_t channel;
    ConvolverWorker work;
    DoubleThreadConvolver* conv2;
    std::atomic<bool> setWait;
    std::chrono::time_point<std::chrono::steady_clock> timePoint;
    std::chrono::microseconds timeoutPeriod;
    bool get_buffer(std::string fname, float **buffer, uint32_t* rate, int* size);
    bool get_buffer(std::string fname, float **buffer1, float **buffer2, uint32_t* rate, int* size);
};


class SelectConvolver
{
public:
    void set_convolver(bool IsPowerOfTwo_) {
            IsPowerOfTwo = IsPowerOfTwo_;
            delete conv;
            conv = IsPowerOfTwo ?
            dynamic_cast<ConvolverBase*>(new GxConvolver()) : 
            dynamic_cast<ConvolverBase*>(new DoubleThreadConvolver());
            }

    bool start(int32_t policy, int32_t priority) {
            return conv->start(policy,priority);}

    bool configure(std::string fname, float gain, unsigned int delay, unsigned int offset,
                            unsigned int length, unsigned int size, unsigned int bufsize) { 
            return conv->configure(fname, gain, delay, offset, length, size, bufsize);}

    bool compute(int32_t count, float* input, float *output) {
            return conv->compute(count, input, output);}

    bool configure(std::string fname, float gain, float lgain,
        unsigned int delay, unsigned int ldelay, unsigned int offset,
        unsigned int length, unsigned int size, unsigned int bufsize) {
            return conv->configure(fname, gain, lgain, delay, ldelay,
                                    offset, length, size, bufsize);}

    bool compute(int count, float* input1, float *input2, float *output1, float *output2) {
            return conv->compute(count, input1, input2, output1, output2);}

    bool checkstate() {
            return conv->checkstate();}

    inline void set_not_runnable() {
            return conv->set_not_runnable();}

    inline bool is_runnable() {
            return conv->is_runnable();}

    inline void set_buffersize(uint32_t sz) {
            return conv->set_buffersize(sz);}

    inline void set_samplerate(uint32_t sr) {
            return conv->set_samplerate(sr);}

    int stop_process() {
            return conv->stop_process();}

    int cleanup() {
            return conv->cleanup();}

    SelectConvolver()
        : IsPowerOfTwo(false) { conv = new ConvolverBase();}

    ~SelectConvolver() { delete conv;}
private:
    bool IsPowerOfTwo;
    ConvolverBase *conv;
};

#endif  // SRC_HEADERS_GX_CONVOLVER_H_
