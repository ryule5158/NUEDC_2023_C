function verify_pll
%VERIFY_PLL Independent numerical checks for the SoftPll ADC/AD9910 design.
% This validates equations and edge mappings; it is not hardware-in-loop proof.

rng(20260721);
check_phase_case(100e6, 40e6, false);  % direct sampling upper requirement
check_phase_case(25e6, 40e6, true);   % band-pass alias with inversion

ftw = uint32(round(40e6 / 1e9 * 2^32));
pow = uint16(mod(round(90 / 360 * 2^16), 2^16));
assert(ftw == uint32(171798692), 'AD9910 FTW rounding failed');
assert(pow == uint16(16384), 'AD9910 POW rounding failed');

check_loop_convergence;
fprintf('verify_pll: all numerical checks passed.\n');
end

function check_phase_case(fs, physical_frequency, bandpass)
n = 0:4095;
reference_phase = 37.0;
feedback_phase = -23.0;
reference = 1.65 + 0.60*cos(2*pi*physical_frequency*n/fs + ...
    deg2rad(reference_phase)) + 1e-3*randn(size(n));
feedback = 1.20 + 0.50*cos(2*pi*physical_frequency*n/fs + ...
    deg2rad(feedback_phase)) + 1e-3*randn(size(n));

if bandpass
    remainder = mod(physical_frequency, fs);
    if remainder > fs/2
        alias = fs - remainder;
        alias_sign = -1;
    else
        alias = remainder;
        alias_sign = 1;
    end
else
    assert(physical_frequency < fs/2, 'Direct sampling violates Nyquist');
    alias = physical_frequency;
    alias_sign = 1;
end

[measured_reference, measured_feedback] = frame_iq_like_c(...
    single(reference), single(feedback), single(fs), single(alias), alias_sign);
measured_error = wrap180(measured_reference-measured_feedback);
expected_error = wrap180(reference_phase-feedback_phase);
assert(abs(wrap180(measured_error-expected_error)) < 0.10, ...
    'Phase detector error exceeds 0.1 degree');
end

function [reference_phase, feedback_phase] = frame_iq_like_c(reference, feedback, fs, alias, alias_sign)
% Single-precision recurrence and one-pass DC correction used by SoftPll.c.
count = numel(reference);
c = single(1); s = single(0);
wc = single(1); ws = single(0);
step = single(2*pi)*alias/fs;
cs = cos(step); ss = sin(step);
wcs = cos(single(2*pi)/single(count-1));
wss = sin(single(2*pi)/single(count-1));
rsum = single(0); rcomp = single(0);
fsum = single(0); fcomp = single(0);
ri = single(0); rq = single(0); fi = single(0); fq = single(0);
wsum = single(0); wcos_sum = single(0); wsin_sum = single(0);

for index = 1:count
    window = single(0.5)-single(0.5)*wc;
    r = reference(index); f = feedback(index);

    corrected = r-rcomp; updated = rsum+corrected;
    rcomp = (updated-rsum)-corrected; rsum = updated;
    corrected = f-fcomp; updated = fsum+corrected;
    fcomp = (updated-fsum)-corrected; fsum = updated;

    ri = ri+window*r*c; rq = rq-window*r*s;
    fi = fi+window*f*c; fq = fq-window*f*s;
    wsum = wsum+window;
    wcos_sum = wcos_sum+window*c;
    wsin_sum = wsin_sum+window*s;

    next_c = c*cs-s*ss; next_s = s*cs+c*ss;
    next_wc = wc*wcs-ws*wss; next_ws = ws*wcs+wc*wss;
    c = next_c; s = next_s; wc = next_wc; ws = next_ws;
    if mod(index, 256) == 0
        scale = single(1)/sqrt(c*c+s*s); c = c*scale; s = s*scale;
        scale = single(1)/sqrt(wc*wc+ws*ws); wc = wc*scale; ws = ws*scale;
    end
end

rmean = rsum/single(count); fmean = fsum/single(count);
ri = ri-rmean*wcos_sum; rq = rq+rmean*wsin_sum;
fi = fi-fmean*wcos_sum; fq = fq+fmean*wsin_sum;
reference_phase = wrap180(alias_sign*rad2deg(atan2(double(rq), double(ri))));
feedback_phase = wrap180(alias_sign*rad2deg(atan2(double(fq), double(fi))));
assert(wsum > 0, 'Invalid Hann coherent gain');
end

function check_loop_convergence
fs = 100e6;
points = 4096;
dt = points/fs;
bandwidth = 200;
damping = 0.70710678;
kp = damping*2*pi*bandwidth/180;
ki = (2*pi*bandwidth)^2/360;
nominal = 40e6;
reference_frequency = nominal + 2000;
capture = min(0.001*nominal, 0.40/dt);
frequency_command = nominal;
integral_hz = 0;
phase_error = 0;

for frame = 1:1000
    phase_error = wrap180(phase_error + ...
        360*(reference_frequency-frequency_command)*dt);
    integral_hz = min(max(integral_hz + ki*phase_error*dt, -capture), capture);
    frequency_command = nominal + kp*phase_error + integral_hz;
    frequency_command = min(max(frequency_command, nominal-capture), ...
                            nominal+capture);
end

assert(abs(frequency_command-reference_frequency) < 0.01, ...
    'PI loop did not remove frequency error');
assert(abs(phase_error) < 0.01, 'PI loop did not remove phase error');
end

function y = wrap180(x)
y = mod(x+180, 360)-180;
end
