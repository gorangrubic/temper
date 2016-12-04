import("stdfaust.lib");

pdrive = hslider("Drive", 1.0, 0.0, 4.0, 0.001);
poffset = hslider("Offset", 0.0, -1.0, 1.0, 0.001);
psmooth = hslider("Smoothing", 0.5, 0.0, 1.0, 0.001);
ptype = hslider("Filter Type", 0, 0, 1, 1.0);
ptransfer = hslider("Transfer Type", 0, 0, 3, 1.0);
 
// Currently supporting the identity function, tanh, and two chebychev polynomial
// transfer functions for waveshaping the modulator.
transfer = _ <: _,ma.tanh,ma.chebychev(2),ma.chebychev(4) : ba.selectn(4, ptransfer);

// Both the one-zero and the allpass filter are stable for `|m(x)| <= 1`, so
// we clamp the driven input signal here.
drive(x) = x : *(pdrive) : +(poffset) : min(1.0) : max(-1.0);

// This signal drives the coefficients of the filter.
m(x) = drive(x) : transfer : si.smooth(psmooth);

// These could all be implemented as a `tf1`, and just map the coefficients according
// to the ptype value?
mzero(x) = x : fi.zero(m(x));

mallpass(x) = x : fi.tf1(m(x), 1.0, m(x));

mfilter = _ <: select2(ptype, mzero, mallpass);

process = mfilter,mfilter;