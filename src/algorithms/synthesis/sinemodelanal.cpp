/*
 * Copyright (C) 2006-2013  Music Technology Group - Universitat Pompeu Fabra
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

#include "sinemodelanal.h"
#include "essentiamath.h"

using namespace essentia;
using namespace standard;

const char* SineModelAnal::name = "SineModelAnal";
const char* SineModelAnal::description = DOC("This algorithm computes the sine model analysis without sine tracking. \n"
"\n"
"It is recommended that the input \"spectrum\" be computed by the Spectrum algorithm. This algorithm uses PeakDetection. See documentation for possible exceptions and input requirements on input \"spectrum\".\n"
"\n"
"References:\n"
"  [1] Peak Detection,\n"
"  http://ccrma.stanford.edu/~jos/parshl/Peak_Detection_Steps_3.html");


// TODO: process multiple frames at the same time to do tracking in standard mode.
// Initial implementation witohut tracking for both standard and streaming mode.
// Next step: implement sine tracking for standard implementation, if we have access to all spectrogram.


// ------------------
// Additional support functions

// sort indexes: get arguments of sorted vector
typedef std::pair<int,Real> mypair;
bool comparator ( const mypair& l, const mypair& r)
{ return l.second < r.second; }

// It sorts the indexes of an input vector v, and outputs the sorted index vector idx
void sort_indexes(std::vector<int> &idx, const std::vector<Real> &v) {
  
  // initialize original index locations
  std::vector<mypair> pairs(v.size());
  for (int i = 0; i != pairs.size(); ++i){
    pairs[i].first = i;
    pairs[i].second = v[i];
  }
  
  // sort indexes based on comparing values in v
  sort(pairs.begin(), pairs.end(),comparator);
  
  // copy sorted indexes
  for (int i = 0; i != pairs.size(); ++i) idx.push_back(pairs[i].first);
  
  return;
}

void copy_vector_from_indexes(std::vector<Real> &out, const std::vector<Real> v, const std::vector<int> idx){

  for (int i = 0; i < idx.size(); ++i){
    out.push_back(v[idx[i]]);
  }
  return;
}

void copy_int_vector_from_indexes(std::vector<int> &out, const std::vector<int> v, const std::vector<int> idx){

  for (int i = 0; i < idx.size(); ++i){
    out.push_back(v[idx[i]]);
  }
  return;
}


void erase_vector_from_indexes(std::vector<Real> &v, const std::vector<int> idx){
  for (int i = 0; i < idx.size(); ++i){
    v.erase(v.begin() + idx[i]);
  }
  return;
}


// ------------------




void SineModelAnal::configure() {

  std::string orderBy = parameter("orderBy").toLower();
  if (orderBy == "magnitude") {
    orderBy = "amplitude";
  }
  else if (orderBy == "frequency") {
    orderBy = "position";
  }
  else {
    throw EssentiaException("Unsupported ordering type: '" + orderBy + "'");
  }

  _peakDetect->configure("interpolate", true,
                         "range", parameter("sampleRate").toReal()/2.0,
                         "maxPeaks", parameter("maxPeaks"),
                         "minPosition", parameter("minFrequency"),
                         "maxPosition", parameter("maxFrequency"),
                         "threshold", parameter("magnitudeThreshold"),
                         "orderBy", orderBy);

}



void SineModelAnal::compute() {
  
  const std::vector<std::complex<Real> >& fft = _fft.get();
  std::vector<Real>& peakMagnitude = _magnitudes.get();
  std::vector<Real>& peakFrequency = _frequencies.get();
  std::vector<Real>& peakPhase = _phases.get();
  
  std::vector<Real> fftmag;
  std::vector<Real> fftphase;
  
  _cartesianToPolar->input("complex").set(fft);
  _cartesianToPolar->output("magnitude").set(fftmag);
  _cartesianToPolar->output("phase").set(fftphase);
  _peakDetect->input("array").set(fftmag);
  _peakDetect->output("positions").set(peakFrequency);
  _peakDetect->output("amplitudes").set(peakMagnitude);
  
  _cartesianToPolar->compute();
  _peakDetect->compute();
  
  phaseInterpolation(fftphase, peakFrequency, peakPhase);
}


// ---------------------------
// additional methods

void SineModelAnal::sinusoidalTracking(std::vector<Real>& peakMags, std::vector<Real>& peakFrequencies, std::vector<Real>& peakPhases, const std::vector<Real> tfreq, Real freqDevOffset, Real freqDevSlope, std::vector<Real> &tfreqn, std::vector<Real> &tmagn, std::vector<Real> &tphasen ){

  //	pfreq, pmag, pphase: frequencies and magnitude of current frame
//	tfreq: frequencies of incoming tracks from previous frame
//	freqDevOffset: minimum frequency deviation at 0Hz
//	freqDevSlope: slope increase of minimum frequency deviation

  freqDevOffset=20;
  freqDevSlope=0.01;


  // sort current peaks per magnitude


// -----
// init arrays

//	tfreqn = np.zeros(tfreq.size)                              # initialize array for output frequencies
//	tmagn = np.zeros(tfreq.size)                               # initialize array for output magnitudes
//	tphasen = np.zeros(tfreq.size)                             # initialize array for output phases
    tfreqn.resize (tfreq.size());
    tmagn.resize (tfreq.size());
    tphasen.resize (tfreq.size());

//	pindexes = np.array(np.nonzero(pfreq), dtype=np.int)[0]    # indexes of current peaks
  std::vector<int> pindexes;
  for (int i=0;i < peakFrequencies.size(); ++i){  if (peakFrequencies[i] > 0) pindexes.push_back(i); }
  
//	incomingTracks = np.array(np.nonzero(tfreq), dtype=np.int)[0] # indexes of incoming tracks
  std::vector<Real> incomingTracks ;
  for (int i=0;i < tfreq.size(); ++i){ if (tfreq[i]>0) incomingTracks.push_back(i); }
  //	newTracks = np.zeros(tfreq.size, dtype=np.int) -1           # initialize to -1 new tracks
  std::vector<int> newTracks(tfreq.size(), -1);
  
  //	magOrder = np.argsort(-pmag[pindexes])                      # order current peaks by magnitude
  std::vector<int> magOrder;
  sort_indexes(magOrder, peakMags);
  
  // copy temporary arrays (as reference)
  //	pfreqt = np.copy(pfreq)                                     # copy current peaks to temporary array
  //	pmagt = np.copy(pmag)                                       # copy current peaks to temporary array
  //	pphaset = np.copy(pphase)                                   # copy current peaks to temporary array
  std::vector<Real>	&pfreqt = peakFrequencies;
  std::vector<Real>	&pmagt = peakMags;
  std::vector<Real>	&pphaset = peakPhases;

  

// -----
// loop for current peaks

//	# continue incoming tracks
//	if incomingTracks.size > 0:                                 # if incoming tracks exist
//		for i in magOrder:                                        # iterate over current peaks
//			if incomingTracks.size == 0:                            # break when no more incoming tracks
//				break
//			track = np.argmin(abs(pfreqt[i] - tfreq[incomingTracks]))   # closest incoming track to peak
//			freqDistance = abs(pfreq[i] - tfreq[incomingTracks[track]]) # measure freq distance
//			if freqDistance < (freqDevOffset + freqDevSlope * pfreq[i]):  # choose track if distance is small
//					newTracks[incomingTracks[track]] = i                      # assign peak index to track index
//					incomingTracks = np.delete(incomingTracks, track)         # delete index of track in incomming tracks

  if (incomingTracks.size() > 0 ){
    int i,j;
    int closestIdx;
    Real freqDistance;
    
    for (j=0; j < magOrder.size(); j++) {
      i = magOrder[j]; // sorted peak index
      if (incomingTracks.size() == 0)
        break; // all tracks have been processed
      
      // find closest peak to incoming track
      closestIdx = 0;
      freqDistance = 1e10;
      for (int k=0; k < incomingTracks.size(); ++k){
        if (freqDistance < std::abs(pfreqt[i] - tfreq[incomingTracks[k]])){
          freqDistance = std::abs(pfreqt[i] - tfreq[incomingTracks[k]]);
          closestIdx = k;
        }
      }
      if (freqDistance < (freqDevOffset + freqDevSlope * peakFrequencies[i])) //  # choose track if distance is small
      {
        newTracks[incomingTracks[closestIdx]] = i;     //     # assign peak index to track index
        incomingTracks.erase(incomingTracks.begin() + closestIdx);              // # delete index of track in incomming tracks
      }
    }
  }
  
          
  //	indext = np.array(np.nonzero(newTracks != -1), dtype=np.int)[0]   # indexes of assigned tracks
          std::vector<int> indext;
          for (int i=0; i < newTracks.size(); ++i)
          {
            if (newTracks[i] != -1) indext.push_back(i);
          }
  //	if indext.size > 0:
        if (indext.size() > 0)
        {
          //		indexp = newTracks[indext]                                    # indexes of assigned peaks
          std::vector<int> indexp;
          copy_int_vector_from_indexes(indexp, newTracks, indext);
          //		tfreqn[indext] = pfreqt[indexp]                               # output freq tracks
          //		tmagn[indext] = pmagt[indexp]                                 # output mag tracks
          //		tphasen[indext] = pphaset[indexp]                             # output phase tracks
          for (int i=0; i < indexp.size(); ++i){
            tfreqn[indext[i]] = pfreqt[indexp[i]];                           //    # output freq tracks
            tmagn[indext[i]] = pmagt[indexp[i]];                             //    # output mag tracks
            tphasen[indext[i]] = pphaset[indexp[i]];                         //    # output phase tracks
          }
          
          //		pfreqt= np.delete(pfreqt, indexp)                             # delete used peaks
          //		pmagt= np.delete(pmagt, indexp)                               # delete used peaks
          //		pphaset= np.delete(pphaset, indexp)                           # delete used peaks
          
          // delete used peaks
          erase_vector_from_indexes(pfreqt, indexp);
          erase_vector_from_indexes(pmagt, indexp);
          erase_vector_from_indexes(pphaset, indexp);
        }

          
  

  // -----
  // create new tracks for non used peaks

//	# create new tracks from non used peaks
//	emptyt = np.array(np.nonzero(tfreq == 0), dtype=np.int)[0]      # indexes of empty incoming tracks
//	peaksleft = np.argsort(-pmagt)                                  # sort left peaks by magnitude
  
  //	emptyt = np.array(np.nonzero(tfreq == 0), dtype=np.int)[0]      # indexes of empty incoming tracks
  std::vector<int> emptyt;
  for (int i=0; i < emptyt.size(); ++i)
  {
    if (tfreq[i] == 0) emptyt.push_back(i);
  }

  
  
 
  //	peaksleft = np.argsort(-pmagt)                                  # sort left peaks by magnitude
  std::vector<int> peaksleft;
  sort_indexes(peaksleft, pmagt);
  

  
//	if ((peaksleft.size > 0) & (emptyt.size >= peaksleft.size)):    # fill empty tracks
//			tfreqn[emptyt[:peaksleft.size]] = pfreqt[peaksleft]
//			tmagn[emptyt[:peaksleft.size]] = pmagt[peaksleft]
//			tphasen[emptyt[:peaksleft.size]] = pphaset[peaksleft]
  
  if ((peaksleft.size() > 0) && (emptyt.size() >= peaksleft.size())){    // fill empty tracks
    
    for (int i=0;i < peaksleft.size(); i++)
    {
      tfreqn[emptyt[i]] = pfreqt[i];
      tmagn[emptyt[i]] = pmagt[i];
      tphasen[emptyt[i]] = pphaset[i];
    }
  
  }
//	elif ((peaksleft.size > 0) & (emptyt.size < peaksleft.size)):   # add more tracks if necessary
  //			tfreqn[emptyt] = pfreqt[peaksleft[:emptyt.size]]
  //			tmagn[emptyt] = pmagt[peaksleft[:emptyt.size]]
  //			tphasen[emptyt] = pphaset[peaksleft[:emptyt.size]]
  //			tfreqn = np.append(tfreqn, pfreqt[peaksleft[emptyt.size:]])
  //			tmagn = np.append(tmagn, pmagt[peaksleft[emptyt.size:]])
  //			tphasen = np.append(tphasen, pphaset[peaksleft[emptyt.size:]])

  else {
    if  ((peaksleft.size() > 0) && (emptyt.size() < peaksleft.size())) { //  add more tracks if necessary

      for (int i=0;i < emptyt.size(); i++)
      {
        tfreqn[i] = pfreqt[peaksleft[i]];
        tmagn[i] = pmagt[peaksleft[i]];
        tphasen[i] = pphaset[peaksleft[i]];
      }
      for (int i=emptyt.size();i < peaksleft.size(); i++)
      {
        tfreqn.push_back(pfreqt[peaksleft[i]]);
        tmagn.push_back(pmagt[peaksleft[i]]);
        tphasen.push_back(pphaset[peaksleft[i]]);
      }
    }
  }

  
  
//	return tfreqn, tmagn, tphasen
//


 // TODO: if multiple frames as input are given. Only supported in standard mode.


}



void SineModelAnal::phaseInterpolation(std::vector<Real> fftphase, std::vector<Real> peakFrequencies, std::vector<Real>& peakPhases){

  int N = peakFrequencies.size();
  peakPhases.resize(N);

  int idx;
  float  a, pos;
  int fftSize = fftphase.size();

  for (int i=0; i < N; ++i){
    // linear interpolation. (as done in numpy.interp function)
    pos =  fftSize * (peakFrequencies[i] / (parameter("sampleRate").toReal()/2.0) );
    idx = int ( 0.5 + pos ); // closest index

    a = pos - idx; // interpolate factor
    // phase diff smaller than PI to do intperolation and avoid jumps
    if (a < 0 && idx > 0){
      peakPhases[i] =  (std::abs(fftphase[idx-1] - fftphase[idx]) > Real(M_PI)) ? a * fftphase[idx-1] + (1.0 -a) * fftphase[idx] : fftphase[idx];
    }
    else {
      if (idx < fftSize-1 ){
        peakPhases[i] = (std::abs(fftphase[idx+1] - fftphase[idx]) > Real(M_PI)) ? a * fftphase[idx+1] + (1.0 -a) * fftphase[idx]: fftphase[idx];
      }
      else {
       peakPhases[i] = fftphase[idx];
     }
    }
  }
}

