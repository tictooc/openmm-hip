/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2009-2015 Stanford University and the Authors.      *
 * Portions copyright (C) 2021 Advanced Micro Devices, Inc. All Rights        *
 * Reserved.                                                                  *
 * Authors: Peter Eastman, Nicholas Curtis                                    *
 * Contributors:                                                              *
 *                                                                            *
 * This program is free software: you can redistribute it and/or modify       *
 * it under the terms of the GNU Lesser General Public License as published   *
 * by the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU Lesser General Public License for more details.                        *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
 * -------------------------------------------------------------------------- */

#include "HipFFTImplHipFFT.h"
#include "HipContext.h"


using namespace OpenMM;
using namespace std;

HipFFTImplHipFFT::HipFFTImplHipFFT(HipContext& context, int xsize, int ysize, int zsize, bool realToComplex, hipStream_t stream, HipArray& in, HipArray& out) :
        HipFFTBase(context, xsize, ysize, zsize, realToComplex, stream, in, out) {

    hipfftResult result;
    result = hipfftPlan3d(&fftForward, xsize, ysize, zsize, context.getUseDoublePrecision() ? HIPFFT_D2Z : HIPFFT_R2C);
    if (result != HIPFFT_SUCCESS)
        throw OpenMMException("Error initializing FFT: "+context.intToString(result));
    result = hipfftPlan3d(&fftBackward, xsize, ysize, zsize, context.getUseDoublePrecision() ? HIPFFT_Z2D : HIPFFT_C2R);
    if (result != HIPFFT_SUCCESS)
        throw OpenMMException("Error initializing FFT: "+context.intToString(result));
    hipfftSetStream(fftForward, stream);
    hipfftSetStream(fftBackward, stream);
}

void HipFFTImplHipFFT::execFFT(bool forward) {
    hipfftResult result = HIPFFT_SUCCESS;
    if (forward) {
        if (context.getUseDoublePrecision()) {
            result = hipfftExecD2Z(fftForward, (double*) in.getDevicePointer(), (double2*) out.getDevicePointer());
        } else {
            result = hipfftExecR2C(fftForward, (float*) in.getDevicePointer(), (float2*) out.getDevicePointer());
        }
    }
    else {
        if (context.getUseDoublePrecision()) {
            result = hipfftExecZ2D(fftBackward, (double2*) out.getDevicePointer(), (double*) in.getDevicePointer());
        } else {
            result = hipfftExecC2R(fftBackward, (float2*) out.getDevicePointer(), (float*) in.getDevicePointer());
        }
    }
    if (result != HIPFFT_SUCCESS)
        throw OpenMMException("Error executing hipFFT: "+context.intToString(result));
}

HipFFTImplHipFFT::~HipFFTImplHipFFT() {
    hipfftDestroy(fftForward);
    hipfftDestroy(fftBackward);
}

int HipFFTImplHipFFT::findLegalDimension(int minimum) {
    if (minimum < 1)
        return 1;
    while (true) {
        // Attempt to factor the current value.

        int unfactored = minimum;
        // HIP-TODO: rocFFT calculates incorrect results for some FFT sizes. It looks like factor == 7 is buggy.
        // Remove this workaround when it's fixed.
        for (int factor = 2; factor < 6/* 8 */; factor++) {
            while (unfactored > 1 && unfactored%factor == 0)
                unfactored /= factor;
        }
        if (unfactored == 1)
            return minimum;
        minimum++;
    }
}
