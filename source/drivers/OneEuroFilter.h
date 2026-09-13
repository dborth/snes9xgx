/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OneEuroFilter.h
 *
 * Adaptive low-pass filter for noisy pointer/position signals, per Casiez,
 * Roussel & Vogel, "1€ Filter: A Simple Speed-based Low-pass Filter for
 * Noisy Input in Interactive Systems" (CHI 2012).
 *
 * A fixed-alpha exponential moving average forces a single tradeoff between
 * jitter (too little smoothing) and lag (too much smoothing) for every speed
 * of movement. This filter instead smooths the *signal* heavily when it's
 * nearly still (killing jitter) and smooths it far less when it's moving
 * fast (killing lag), by feeding a filtered estimate of speed back into the
 * cutoff frequency used to filter the signal itself. It needs no platform
 * headers and only a handful of multiplies per sample, so it's cheap enough
 * to run per-axis, per-channel, per-frame on devkitPPC.
 *
 * Two knobs:
 *   minCutoff - cutoff frequency (Hz) when speed is ~0. Lower = smoother at
 *               rest, but more lag when motion starts. This is the main
 *               "jitter vs lag" knob.
 *   beta      - how much cutoff increases with speed. Higher = snappier
 *               during fast motion, but the filter tolerates more residual
 *               jitter as speed rises to get there.
 *
 * dCutoff (derivative cutoff) rarely needs tuning away from 1.0.
 ***************************************************************************/
#pragma once

#include <cmath>

class LowPassFilter {
public:
	LowPassFilter() : initialized(false), storedValue(0.0f) {}

	float filter(float value, float alpha) {
		if (!initialized) {
			storedValue = value;
			initialized = true;
		} else {
			storedValue = alpha * value + (1.0f - alpha) * storedValue;
		}
		return storedValue;
	}

	float lastValue() const { return storedValue; }
	void reset() { initialized = false; storedValue = 0.0f; }

private:
	bool initialized;
	float storedValue;
};

class OneEuroFilter {
public:
	OneEuroFilter(float minCutoff = 1.0f, float beta = 0.0f, float dCutoff = 1.0f)
		: minCutoff(minCutoff), beta(beta), dCutoff(dCutoff), firstSample(true) {}

	// value: raw noisy sample. dt: seconds since the previous call (must be > 0).
	float filter(float value, float dt) {
		if (dt <= 0.0f) dt = 1.0f / 60.0f;

		if (firstSample) {
			firstSample = false;
			xFilter.filter(value, 1.0f);
			dxFilter.filter(0.0f, 1.0f);
			return value;
		}

		// Estimate (filtered) speed of the signal.
		float dx = (value - xFilter.lastValue()) / dt;
		float edx = dxFilter.filter(dx, computeAlpha(dCutoff, dt));

		// Cutoff rises with speed: fast movement gets less smoothing (less
		// lag), near-zero movement gets the full minCutoff smoothing (less
		// jitter).
		float cutoff = minCutoff + beta * std::fabs(edx);

		return xFilter.filter(value, computeAlpha(cutoff, dt));
	}

	void reset() {
		firstSample = true;
		xFilter.reset();
		dxFilter.reset();
	}

	void setParams(float newMinCutoff, float newBeta, float newDCutoff = 1.0f) {
		minCutoff = newMinCutoff;
		beta = newBeta;
		dCutoff = newDCutoff;
	}

private:
	static float computeAlpha(float cutoff, float dt) {
		float tau = 1.0f / (2.0f * (float)M_PI * cutoff);
		return 1.0f / (1.0f + tau / dt);
	}

	float minCutoff;
	float beta;
	float dCutoff;
	bool firstSample;
	LowPassFilter xFilter;
	LowPassFilter dxFilter;
};
