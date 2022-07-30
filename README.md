# qstdcdec
QT version of stdcdec_parser

Requires stream of decoded samples at the input

Related projects:

    inmarsatc: library with all functions to receive Inmarsat-C signals
    https://github.com/cropinghigh/inmarsatc

    stdcdec: set of programs to receive inmarsat-c signals
    https://github.com/cropinghigh/stdcdec

    sdrpp-inmarsatc-demodulator: SDR++ module, which replaces stdcdec_demod and stdcdec_decoder
    https://github.com/cropinghigh/sdrpp-inmarsatc-demodulator

Building:

          mkdir build
          cd build
          cmake ..
          make
          sudo make install

Usage:
  1.  Run stdcdec_demod->stdcdec_decoder chain or use the sdrpp-inmarsatc-demodulator module to get UDP stream of frames
  2.  Select same port in the qstdcdec and start listening
  3.  You will seee decoded packets and messages

