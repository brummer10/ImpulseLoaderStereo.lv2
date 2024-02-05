# ImpulseLoaderStereo

This is a simple, stereo, IR-File loader/convolution LV2 plug. 

![ImpulseLoaderStereo](https://raw.githubusercontent.com/brummer10/ImpulseLoaderStereo.lv2/master/ImpulseLoaderStereo.png)

## Supported File Formats

- WAF
- AIFF
- WAVEFORMATEX
- CAF

IR-Files could be loaded via the integrated File Browser, or, when supported by the host, via drag and drop.

If the IR-File have only 1 channel, it will be copied to the second channel.

If the IR-File have more the 2 channels, only the first 2 channels will be loaded.

IR-Files will be resampled on the fly to match the session Sample Rate.

## Dependencies

- libsndfile1-dev
- libfftw3-dev
- libcairo2-dev
- libx11-dev
- lv2-dev

## Build

- git submodule init
- git submodule update
- make
- make install # will install into ~/.lv2 ... AND/OR....
- sudo make install # will install into /usr/lib/lv2

