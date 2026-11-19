#pragma once

#include <cstdint>

#if defined(__DOS__)

namespace DosMath {
	static float fmin(float a, float b) {
	    return ((a > b) ? (b) : (a));
	}
	
	static float fmax(float a, float b) {
	    return ((a > b) ? (a) : (b));
	}
	
	static double round(double val) {
	    uint64_t int_val = static_cast<int>(val);
	    double decimal = val - int_val;
	    if (decimal < 0.5) {
	        return val - decimal;
	    } else {
	        return (val - decimal) + 1;
	    }
	}
	
	static  double clamp(double val, double a, double b) {
		if (val > a) return a;
		if (val < b) return b;
	
		return val; 
	}
}

#endif