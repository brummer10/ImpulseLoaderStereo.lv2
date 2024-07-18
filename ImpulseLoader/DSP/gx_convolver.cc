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

#include "gx_convolver.h"
#include <string.h>
/****************************************************************
 ** some pieces in this file are copied from jconvolver
 */

#define max(x, y) (((x) > (y)) ? (x) : (y))
/****************************************************************
 ** GxConvolverBase
 */

Audiofile::Audiofile(void) {
    reset();
}


Audiofile::~Audiofile(void) {
    close();
}


void Audiofile::reset(void) {
    _sndfile = 0;
    _type = TYPE_OTHER;
    _form = FORM_OTHER;
    _rate = 0;
    _chan = 0;
    _size = 0;
}


int Audiofile::open_read(std::string name) {
    SF_INFO I;

    reset();

    if ((_sndfile = sf_open(name.c_str(), SFM_READ, &I)) == 0) return ERR_OPEN;

    switch (I.format & SF_FORMAT_TYPEMASK) {
    case SF_FORMAT_CAF:
        _type = TYPE_CAF;
        break;
    case SF_FORMAT_WAV:
        _type = TYPE_WAV;
        break;
    case SF_FORMAT_AIFF:
        _type = TYPE_AIFF;
        break;
    case SF_FORMAT_WAVEX:
#ifdef     SFC_WAVEX_GET_AMBISONIC
        if (sf_command(_sndfile, SFC_WAVEX_GET_AMBISONIC, 0, 0) == SF_AMBISONIC_B_FORMAT)
            _type = TYPE_AMB;
        else
#endif
            _type = TYPE_WAV;
    }

    switch (I.format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_16:
        _form = FORM_16BIT;
        break;
    case SF_FORMAT_PCM_24:
        _form = FORM_24BIT;
        break;
    case SF_FORMAT_PCM_32:
        _form = FORM_32BIT;
        break;
    case SF_FORMAT_FLOAT:
        _form = FORM_FLOAT;
        break;
    }

    _rate = I.samplerate;
    _chan = I.channels;
    _size = I.frames;

    return 0;
}


int Audiofile::close(void) {
    if (_sndfile) sf_close(_sndfile);
    reset();
    return 0;
}


int Audiofile::seek(uint32_t posit) {
    if (!_sndfile) return ERR_MODE;
    if (sf_seek(_sndfile, posit, SEEK_SET) != posit) return ERR_SEEK;
    return 0;
}


int Audiofile::read(float *data, uint32_t frames) {
    return sf_readf_float(_sndfile, data, frames);
}

GxConvolverBase::~GxConvolverBase()
{
  if (is_runnable())
    {
      stop_process();
    }
}

void GxConvolverBase::adjust_values(
  uint32_t audio_size, uint32_t& count, uint32_t& offset,
  uint32_t& delay, uint32_t& ldelay, uint32_t& length,
  uint32_t& size, uint32_t& bufsize)
{

  if (bufsize < count)
    {
      bufsize = count;
    }
  if (bufsize < Convproc::MINPART)
    {
      bufsize = Convproc::MINPART;
    }
  if ((bufsize & (bufsize - 1)) != 0) // IsPowerOfTwo
    {
      bufsize--;
      bufsize |= bufsize >> 1;
      bufsize |= bufsize >> 2;
      bufsize |= bufsize >> 4;
      bufsize |= bufsize >> 8;
      bufsize |= bufsize >> 16;
      bufsize++;     
    }
  if (offset > audio_size)
    {
      offset = audio_size;
    }
  if (!size)
    {
      if (offset + length > audio_size)
        {

          length = audio_size - offset;
        }
      if (!length)
        {
          length = audio_size - offset;
        }
      size = max(delay, ldelay) + offset + length;
    }
  else
    {
      if (delay > size)
        {
          delay = size;
        }
      if (ldelay > size)
        {
          ldelay = size;
        }
      if (offset > size - max(delay, ldelay))
        {
          offset = size - max(delay, ldelay);
        }
      if (length > size - max(delay, ldelay) - offset)
        {
          length = size - max(delay, ldelay) - offset;

        }
      if (!length)
        {
          length = size - max(delay, ldelay) - offset;
        }
    }
}

bool GxConvolverBase::start(int32_t policy, int32_t priority)
{
  int32_t rc = start_process(priority, policy);
  if (rc != 0)
    {

      return false;
    }
  ready = true;
  return true;
}

bool GxConvolverBase::checkstate()
{
  if (state() == Convproc::ST_WAIT)
    {
      if (check_stop())
        {
          ready = false;
        }
      else
        {
          return false;
        }
    }
  else if (state() == ST_STOP)
    {
      ready = false;
    }
  return true;
}

/****************************************************************
 ** GxConvolver
 */

/*
** GxConvolver::read_sndfile()
**
** read samples from soundfile into convolver
** the convolver is working at rate samplerate, the audio file has audio.rate
**
** offset, length, points are based on audio.rate, delay and the count of
** samples written into the convolver are based on samplerate.
**
** Arguments:
**    Audiofile& audio        already opened, will be converted to samplerate
**                            on the fly
**    int nchan               channel count for convolver (can be less
**                            or more than audio.chan())
**    int samplerate          current engine samplerate
**    const float *gain       array[nchan] of gains to be applied
**    unsigned int *delay     array[nchan], starting sample index for values
**                            stored into convolver
**    unsigned int offset     offset into audio file
**    unsigned int length     number of samples to be read from audio
**    const Gainline& points  gain line to be applied
**
** returns false if some error occurred, else true
*/
bool GxConvolver::read_sndfile(
    Audiofile& audio, int nchan, int samplerate, const float *gain,
    unsigned int *delay, unsigned int offset, unsigned int length) {
    int nfram;
    float *buff;
    float *rbuff = 0;
    float *bufp;
    // keep BSIZE big enough so that resamp.flush() doesn't cause overflow
    // (> 100 should be enough, and should be kept bigger anyhow)
    const unsigned int BSIZE = 0x8000; //  0x4000;
    

    if (offset && audio.seek(offset)) {
        fprintf(stderr,  "Can't seek to offset\n");
        audio.close();
        return false;
    }
    try {
        buff = new float[BSIZE * audio.chan()];
    } catch(...) {
        audio.close();
        fprintf(stderr,  "out of memory\n");
        return false;
    }
    if (samplerate != audio.rate()) {
        fprintf(stderr, 
		"resampling from %i to %i\n", audio.rate(), samplerate);
        if (!resamp.setup(audio.rate(), samplerate, audio.chan())) {
            fprintf(stderr,  "resample failure\n");
            assert(false);
	    return false;
        }
        try {
            rbuff = new float[resamp.get_max_out_size(BSIZE)*audio.chan()];
        } catch(...) {
            audio.close();
            fprintf(stderr,  "out of memory\n");
            return false;
        }
        bufp = rbuff;
    } else {
        bufp = buff;
    }
    bool done = false;

    while (!done) {
        unsigned int cnt;
        nfram = (length > BSIZE) ? BSIZE : length;
        if (length) {
            nfram = audio.read(buff, nfram);
            if (nfram < 0) {
                fprintf(stderr,  "Error reading file\n");
                audio.close();
                delete[] buff;
                delete[] rbuff;
                return false;
            }
            offset += nfram;
            cnt = nfram;
            if (rbuff) {
                cnt = resamp.process(nfram, buff, rbuff);
            }
        } else {
            if (rbuff) {
                cnt = resamp.flush(rbuff);
                done = true;
            } else {
                break;
            }
        }
        if (cnt) {

            for (int ichan = 0; ichan < nchan; ichan++) {
                int rc;
                if (ichan >= audio.chan()) {
                    rc = impdata_copy(0, 0, ichan, ichan);
                } else {
                    rc = impdata_create(ichan, ichan, audio.chan(), bufp + ichan,
                                        delay[ichan], delay[ichan] + cnt);
                }
                if (rc) {
                    audio.close();
                    delete[] buff;
                    delete[] rbuff;
                    fprintf(stderr,  "out of memory\n");
                    return false;
                }
                delay[ichan] += cnt;
            }
            length -= nfram;
        }
    }

    audio.close();
    delete[] buff;
    delete[] rbuff;
    
    return true;
}

bool GxConvolver::configure(
    std::string fname, float gain, float lgain,
    unsigned int delay, unsigned int ldelay, unsigned int offset,
    unsigned int length, unsigned int size, unsigned int bufsize) {
    Audiofile     audio;
    cleanup();
    if (fname.empty() || !samplerate) {
        return false;
    }
    if (audio.open_read(fname)) {
        fprintf(stderr, "Unable to open '%s'\n", fname.c_str());
        return false;
    }
    if (audio.chan() > 2) {
        fprintf(stderr,"only taking first 2 of %i channels in impulse response\n", audio.chan());
       // return false;
    }
    adjust_values(audio.size(), buffersize, offset, delay, ldelay, length, size, bufsize);

    if (samplerate != static_cast<unsigned int>(audio.rate())) {
	float f = float(samplerate) / audio.rate();
	size = round(size * f) + 2; // 2 is safety margin for rounding differences
	delay = round(delay * f);
	ldelay = round(ldelay * f);
    }
#if ZITA_CONVOLVER_VERSION == 4
        if (Convproc::configure(2, 2, size, buffersize, bufsize, Convproc::MAXPART, 0.0)) {
            fprintf(stderr,  "error in Convproc::configure \n");
            return false;
        }        
#else 
        if (Convproc::configure(2, 2, size, buffersize, bufsize, Convproc::MAXPART)) {
            fprintf(stderr,  "error in Convproc::configure \n");
            return false;
        }
#endif

    float gain_a[2] = {gain, lgain};
    unsigned int delay_a[2] = {delay, ldelay};
    return read_sndfile(audio, 2, samplerate, gain_a, delay_a, offset, length);
}

bool GxConvolver::compute(int count, float* input1, float *input2,
				    float *output1, float *output2) {
    if (state() != Convproc::ST_PROC) {
        if (input1 != output1) {
            memcpy(output1, input1, count * sizeof(float));
        }
        if (input2 != output2) {
            memcpy(output2, input2, count * sizeof(float));
        }
	if (state() == Convproc::ST_WAIT) {
	    check_stop();
	}
        if (state() == ST_STOP) {
            ready = false;
        }
        return true;
    }
    int32_t flags = 0;
    if (static_cast<uint32_t>(count) == buffersize) {
        memcpy(inpdata(0), input1, count * sizeof(float));
        memcpy(inpdata(1), input2, count * sizeof(float));
        flags = process(sync);
        memcpy(output1, outdata(0), count * sizeof(float));
        memcpy(output2, outdata(1), count * sizeof(float));
    } else if (static_cast<uint32_t>(count) < buffersize) {
        float in1[buffersize];
        float in2[buffersize];
        memset(in1, 0, buffersize * sizeof(float));
        memset(in2, 0, buffersize * sizeof(float));
        memcpy(in1, input1, count * sizeof(float));
        memcpy(in2, input2, count * sizeof(float));
        memcpy(inpdata(0), in1, buffersize * sizeof(float));
        memcpy(inpdata(1), in2, buffersize * sizeof(float));
        flags = process(sync);
        memcpy(output1, outdata(0), count * sizeof(float));
        memcpy(output2, outdata(1), count * sizeof(float));
    } else {
        float *in1, *out1, *in2, *out2;
        in1 = inpdata(0);
        out1 = outdata(0);
        in2 = inpdata(1);
        out2 = outdata(1);
        uint32_t b = 0;
        uint32_t d = 0;
        uint32_t s = 0;
        for(int32_t i = 0; i<count; i++) {
            in1[b] = input1[i];
            in2[b] = input2[i];
            if(++b == buffersize) {
                b=0;
                flags = process();
                for(d = 0; d<buffersize; d++, s++) {
                    output1[s] = out1[d];
                    output2[s] = out2[d];
                }
            }
        }
        if (s < static_cast<uint32_t>(count)) {
            int32_t r = static_cast<uint32_t>(count) - s;
            float in1[buffersize];
            float in2[buffersize];
            memset(in1, 0, buffersize * sizeof(float));
            memset(in2, 0, buffersize * sizeof(float));
            memcpy(in1, &input1[s], r * sizeof(float));
            memcpy(in2, &input2[s], r * sizeof(float));
            memcpy(inpdata(0), in1, buffersize * sizeof(float));
            memcpy(inpdata(1), in2, buffersize * sizeof(float));
            flags = process(sync);
            for(int32_t i = 0; i<r; i++, s++) {
                output1[s] = out1[i];
                output2[s] = out2[i];
            }
            //printf("convolver missing %u samples", r);
        }
    }
    return flags == 0;
}

bool GxConvolver::configure(std::string fname, float gain, unsigned int delay, unsigned int offset,
			    unsigned int length, unsigned int size, unsigned int bufsize) {
    Audiofile audio;
    cleanup();
    if (fname.empty() || !samplerate) {
        return false;
    }
    if (audio.open_read(fname)) {
        fprintf(stderr, "Unable to open '%s'\n", fname.c_str());
	return false;
    }
    if (audio.chan() > 1) {
        fprintf(stderr,"only taking first channel of %i channels in impulse response\n", audio.chan());
	//return false;
    }
    unsigned int ldelay = delay;
    adjust_values(audio.size(), buffersize, offset, delay, ldelay, length, size, bufsize);

    if (samplerate != static_cast<unsigned int>(audio.rate())) {
	float f = float(samplerate) / audio.rate();
	size = round(size * f) + 2; // 2 is safety margin for rounding differences
	delay = round(delay * f);
    }
#if ZITA_CONVOLVER_VERSION == 4
        if (Convproc::configure(1, 1, size, buffersize, bufsize, Convproc::MAXPART,0.0)) {
            fprintf(stderr,  "error in Convproc::configure \n");
            return false;
        }        
#else 
        if (Convproc::configure(1, 1, size, buffersize, bufsize, Convproc::MAXPART)) {
            fprintf(stderr,  "error in Convproc::configure \n");
            return false;
        }
#endif

    float gain_a[1] = {gain};
    unsigned int delay_a[1] = {delay};
    return read_sndfile(audio, 1, samplerate, gain_a, delay_a, offset, length);
}

bool GxConvolver::compute(int count, float* input, float *output) {
    if (state() != Convproc::ST_PROC) {
        if (input != output) {
            memcpy(output, input, count * sizeof(float));
        }
	if (state() == Convproc::ST_WAIT) {
	    check_stop();
	}
        if (state() == ST_STOP) {
            ready = false;
        }
        return true;
    }
    memcpy(inpdata(0), input, count * sizeof(float));

    int flags = process(sync);

    memcpy(output, outdata(0), count * sizeof(float));
    return flags == 0;
}


/****************************************************************
 ** DoubleThreadConvolver
 */

///////////////////////// INTERNAL WORKER CLASS   //////////////////////

ConvolverWorker::ConvolverWorker(DoubleThreadConvolver &xr)
    : _execute(false),
    _xr(xr) {
}

ConvolverWorker::~ConvolverWorker() {
    if( _execute.load(std::memory_order_acquire) ) {
        stop();
    };
}

void ConvolverWorker::set_priority() {
#if defined(__linux__) || defined(_UNIX) || defined(__APPLE__)
    sched_param sch_params;
    sch_params.sched_priority = 5;
    if (pthread_setschedparam(_thd.native_handle(), SCHED_FIFO, &sch_params)) {
        fprintf(stderr, "ConvolverWorker: fail to set priority\n");
    }
#elif defined(_WIN32)
    // HIGH_PRIORITY_CLASS, THREAD_PRIORITY_TIME_CRITICAL
    if (SetThreadPriority(_thd.native_handle(), 15)) {
        fprintf(stderr, "ConvolverWorker: fail to set priority\n");
    }
#else
    //system does not supports thread priority!
#endif
}

void ConvolverWorker::stop() {
    if (is_running()) {
        _execute.store(false, std::memory_order_release);
        if (_thd.joinable()) {
            cv.notify_one();
            _thd.join();
        }
    }
}

void ConvolverWorker::run() {
    if( _execute.load(std::memory_order_acquire) ) {
        stop();
    };
    _execute.store(true, std::memory_order_release);
    _thd = std::thread([this]() {
        set_priority();
        while (_execute.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(m);
            // wait for signal from dsp that work is to do
            cv.wait(lk);
            //do work
            if (_execute.load(std::memory_order_acquire)) {
                _xr.doBackgroundProcessing();
                _xr.co.notify_one();
                _xr.setWait.store(false, std::memory_order_release);
            }
        }
        // when done
    });    
}

void ConvolverWorker::start() {
    if (!is_running()) run();
}

bool ConvolverWorker::is_running() const noexcept {
    return ( _execute.load(std::memory_order_acquire) && 
             _thd.joinable() );
}

void DoubleThreadConvolver::startBackgroundProcessing()
{
    if (work.is_running()) {
        timePoint = std::chrono::steady_clock::now() + timeoutPeriod;
        setWait.store(true, std::memory_order_release);
        work.cv.notify_one();
    } else {
        doBackgroundProcessing();
    }
}


void DoubleThreadConvolver::waitForBackgroundProcessing()
{
    if (setWait.load(std::memory_order_acquire)) {
        if (work.is_running()) {
            std::unique_lock<std::mutex> lk(mo);
            if (co.wait_until(lk, timePoint) == std::cv_status::timeout)
                fprintf(stderr, "Convolver: overrun, time out!!\n");
        }
    }
}

bool DoubleThreadConvolver::get_buffer(std::string fname, float **buffer, uint32_t *rate, int *asize)
{
    Audiofile audio;
    if (audio.open_read(fname)) {
        fprintf(stderr, "Unable to open %s\n", fname.c_str() );
        *buffer = 0;
        return false;
    }
    *rate = audio.rate();
    *asize = audio.size();
    const int limit = 2000000; // arbitrary size limit
    if (*asize > limit) {
        fprintf(stderr, "too many samples (%i), truncated to %i\n"
                           , audio.size(), limit);
        *asize = limit;
    }
    if (*asize * audio.chan() == 0) {
        fprintf(stderr, "No samples found\n");
        *buffer = 0;
        audio.close();
        return false;
    }
    float* cbuffer = new float[*asize * audio.chan()];
    if (audio.read(cbuffer, *asize) != static_cast<int>(*asize)) {
        delete[] cbuffer;
        fprintf(stderr, "Error reading file\n");
        *buffer = 0;
        audio.close();
        return false;
    }
    if (audio.chan() > 1) {
        //fprintf(stderr,"only taking first channel of %i channels in impulse response\n", audio.chan());
        float *abuffer = new float[*asize];
        for (int i = 0; i < *asize; i++) {
            abuffer[i] = cbuffer[(i + channel) * audio.chan()];
        }
        delete[] cbuffer;
        cbuffer = NULL;
        cbuffer = abuffer;
    }
    *buffer = cbuffer;
    audio.close();
    if (*rate != samplerate) {
        *buffer = resamp.process(*rate, *asize, *buffer, samplerate, asize);
        if (!*buffer) {
            printf("no buffer\n");
            return false;
        }
        //fprintf(stderr, "FFTConvolver: resampled from %i to %i\n", *rate, samplerate);
    }
    return true;
}


bool DoubleThreadConvolver::configure(std::string fname, float gain, unsigned int delay, unsigned int offset,
            unsigned int length, unsigned int size, unsigned int bufsize)
{
    float* abuf = NULL;
    uint32_t arate = 0;
    int asize = 0;
    if (!get_buffer(fname, &abuf, &arate, &asize)) {
        return false;
    }
    int timeout = max(1000,static_cast<int>((buffersize/(samplerate*0.000001))*0.4));
    timeoutPeriod = std::chrono::microseconds(timeout);
    uint32_t _head = 1;
    while (_head < buffersize) {
        _head *= 2;
    }
    uint32_t _tail = _head > 8192 ? _head : 8192;
    //fprintf(stderr, "head %i tail %i irlen %i timeout %i microsec\n", _head, _tail, asize, timeout);
    if (init(_head, _tail, abuf, asize)) {
        ready = true;
        delete[] abuf;
        return true;
    }
    delete[] abuf;
    return false;
}

bool DoubleThreadConvolver::compute(int32_t count, float* input, float* output)
{
    process(input, output, count);
    return true;
}

bool DoubleThreadConvolver::configure(std::string fname, float gain, float lgain,
        unsigned int delay, unsigned int ldelay, unsigned int offset,
        unsigned int length, unsigned int size, unsigned int bufsize)
{
    if (conv2 == nullptr) {
        conv2 = new DoubleThreadConvolver();
        conv2->set_buffersize(buffersize);
        conv2->set_samplerate(samplerate);
        conv2->channel = 1;
        conv2->start(0,0);
    }
    if (configure(fname, gain, delay, offset, length, size, bufsize))
        return conv2->configure(fname, lgain, ldelay, offset, length, size, bufsize);
    return false;
}

bool DoubleThreadConvolver::compute(int count, float* input1, float *input2, float *output1, float *output2)
{
    process(input1, output1, count);
    conv2->process(input2, output2, count);
    return true;    
}
