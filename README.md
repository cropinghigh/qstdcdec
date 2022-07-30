# qstdcdec
QT version of stdcdec_parser

Requires stream of decoded frames at the input

Related projects:

    inmarsatc: library with all functions to receive Inmarsat-C signals
    https://github.com/cropinghigh/inmarsatc

    stdcdec: set of programs to receive inmarsat-c signals
    https://github.com/cropinghigh/stdcdec

    sdrpp-inmarsatc-demodulator: SDR++ module, which replaces stdcdec_demod and stdcdec_decoder
    https://github.com/cropinghigh/sdrpp-inmarsatc-demodulator

Building:

  1.  Install inmarsatc library(libinmarsatc-git for arch-like systems)

  2.  Build

          mkdir build
          cd build
          cmake ..
          make
          sudo make install

Usage:

  1.  Run stdcdec_demod->stdcdec_decoder chain or use the sdrpp-inmarsatc-demodulator module to get UDP stream of frames

  2.  Select same port in the qstdcdec and start listening

  3.  You will see decoded packets and messages

  WARNING! All messages are directed to their recipients! If you're not the recipient, you should delete received message!

