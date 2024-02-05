// generated from file 'Gain.dsp' by dsp2cc:
// Code generated with Faust 2.69.3 (https://faust.grame.fr)

#include <cmath>

#define FAUSTFLOAT float

namespace gain {

class Dsp {
private:
	uint32_t fSampleRate;
	FAUSTFLOAT fVslider0;
	FAUSTFLOAT	*fVslider0_;
	float fRec0[2];


public:
	void connect(uint32_t port,void* data);
	void del_instance(Dsp *p);
	void clear_state_f();
	void init(uint32_t sample_rate);
	void compute(int count, FAUSTFLOAT *input0, FAUSTFLOAT *input1, FAUSTFLOAT *output0, FAUSTFLOAT *output1);
	Dsp();
	~Dsp();
};



Dsp::Dsp() {
}

Dsp::~Dsp() {
}

inline void Dsp::clear_state_f()
{
	for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec0[l0] = 0.0f;
}

inline void Dsp::init(uint32_t sample_rate)
{
	fSampleRate = sample_rate;
	clear_state_f();
}

void Dsp::compute(int count, FAUSTFLOAT *input0, FAUSTFLOAT *input1, FAUSTFLOAT *output0, FAUSTFLOAT *output1)
{
#define fVslider0 (*fVslider0_)
	float fSlow0 = 0.001f * std::pow(1e+01f, 0.05f * float(fVslider0));
	for (int i0 = 0; i0 < count; i0 = i0 + 1) {
		fRec0[0] = fSlow0 + 0.999f * fRec0[1];
		output0[i0] = FAUSTFLOAT(float(input0[i0]) * fRec0[0]);
		output1[i0] = FAUSTFLOAT(float(input1[i0]) * fRec0[0]);
		fRec0[1] = fRec0[0];
	}
#undef fVslider0
}


void Dsp::connect(uint32_t port,void* data)
{
	switch ((PortIndex)port)
	{
	case 5: 
		fVslider0_ = static_cast<float*>(data); // , 0.0f, -2e+01f, 6.0f, 0.1f 
		break;
	default:
		break;
	}
}

Dsp *plugin() {
	return new Dsp();
}

void Dsp::del_instance(Dsp *p)
{
	delete p;
}
} // end namespace gain
