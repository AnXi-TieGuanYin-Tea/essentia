/*
 * Copyright (C) 2006-2015  Music Technology Group - Universitat Pompeu Fabra
 *
 * This file is part of Essentia
 *
 * Essentia is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation (FSF), either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the Affero GNU General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */

#ifndef ESSENTIA_SINEMODELANAL_H
#define ESSENTIA_SINEMODELANAL_H

#include "algorithm.h"
#include "algorithmfactory.h"


namespace essentia {
namespace standard {

class SineModelAnal : public Algorithm {

 protected:
  Input<std::vector<std::complex<Real> > > _fft;
  Output<std::vector<Real> > _magnitudes;
  Output<std::vector<Real> > _frequencies;
  Output<std::vector<Real> > _phases;
  Algorithm* _peakDetect;
  Algorithm* _cartesianToPolar;

 public:
  SineModelAnal() {
    declareInput(_fft, "fft", "the input frame");
    declareOutput(_frequencies, "frequencies", "the frequencies of the sinusoidal peaks [Hz]");
    declareOutput(_magnitudes, "magnitudes", "the magnitudes of the sinusoidal peaks");
    declareOutput(_phases, "phases", "the phases of the sinusoidal peaks");

    _peakDetect = AlgorithmFactory::create("PeakDetection");
    _cartesianToPolar = AlgorithmFactory::create("CartesianToPolar");

  }

  ~SineModelAnal() {
    delete _peakDetect;
    delete _cartesianToPolar;
  }

  void declareParameters() {
    declareParameter("sampleRate", "the sampling rate of the audio signal [Hz]", "(0,inf)", 44100.);
    declareParameter("maxPeaks", "the maximum number of returned peaks", "[1,inf)", 100);
    declareParameter("maxFrequency", "the maximum frequency of the range to evaluate [Hz]", "(0,inf)", 5000.0);
    declareParameter("minFrequency", "the minimum frequency of the range to evaluate [Hz]", "[0,inf)", 0.0);
    declareParameter("magnitudeThreshold", "peaks below this given threshold are not outputted", "(-inf,inf)", 0.0);
    declareParameter("orderBy", "the ordering type of the outputted peaks (ascending by frequency or descending by magnitude)", "{frequency,magnitude}", "frequency");
  }

  void configure();
  void compute();

  void phaseInterpolation(std::vector<Real> fftphase, std::vector<Real> peakFrequencies, std::vector<Real>& peakPhases);
  void sinusoidalTracking(std::vector<Real>& peakMags, std::vector<Real>& peakFrequencies, std::vector<Real>& peakPhases, const std::vector<Real> tfreq, Real freqDevOffset, Real freqDevSlope, std::vector<Real> &tfreqn, std::vector<Real> &tmagn, std::vector<Real> &tphasen );
  void cleaningSineTrack();
  //std::vector<int> sort_indexes(const std::vector<Real> &v); // maybe move it to utils


  static const char* name;
  static const char* description;

};

} // namespace standard
} // namespace essentia

#include "streamingalgorithmwrapper.h"

namespace essentia {
namespace streaming {

class SineModelAnal : public StreamingAlgorithmWrapper {

 protected:
  Sink<std::vector<std::complex<Real> > > _fft; // input
  Source<std::vector<Real> > _frequencies;
  Source<std::vector<Real> > _magnitudes;
  Source<std::vector<Real> > _phases;

 public:
  SineModelAnal() {
    declareAlgorithm("SineModelAnal");
    declareInput(_fft, TOKEN, "fft");
    declareOutput(_frequencies, TOKEN, "frequencies");
    declareOutput(_magnitudes, TOKEN, "magnitudes");
    declareOutput(_phases, TOKEN, "phases");
  }
};

} // namespace streaming
} // namespace essentia




#endif // ESSENTIA_SINEMODELANAL_H
