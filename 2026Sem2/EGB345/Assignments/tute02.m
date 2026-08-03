%% EGB242 Lab 2 Template
%% (a) Create a time vector t for the input voltage signal using 1000 samples.
samples = 1000;
% Time period
T = 6; 
% time vector from [0, T)
t = linspace(0, 6, samples+1);
t = t(1:end-1);
%% (b) Generate the input signal vin.
vin = zeros(size(t));
% Three segments
vin(0 <= t & t < 2) = 2 * t(0 <= t & t < 2) ; % TODO
vin(2 <= t & t < 4) = 4; % TODO
vin(4 <= t & t < 6) = -2 * t(4 <= t & t < 6) + 12; % TODO
%% (c) Using the expression for vout in terms of vin derived in Question 2
%% of the tutorial, generate the output of the circuit vout.
R1 = 22e3; R2 = 33e3; R3 = 12e3; R4 = 18e3;
vout = vin*R2*R4 / ((R2 + R3 + R4) * (R1 + R2) - R2^2); % TODO
%% (d) Plot the input and output of the circuit on the same set of axes. Include a
%title, X axis label, Y axis label, and legend.
figure; hold on;
% plot v in
plot(t, vin);
% plot vout
plot(t, vout);
title('Simulation of mesh circuit');
xlabel('Time [s]');
ylabel('Voltage [V]');
legend('v_{in}', 'v_{out}');
%% (e)
R1 = 22e3; R2 = 33e3; R3 = 12e3;
R4s = [12e3, 15e3, 18e3, 22e3, 27e3];
N = length(R4s);
% creating an array to store the output for each of the new resistor values
vout = zeros(N, samples);
% Loop over all of these new resistors and calculate
for i = 1:N
R4 = R4s(i);
vout(i, :) = []; % TODO
end
%% (e) extended - plot new simulations
figure; hold on;
plot(t, vin);
plot(t, vout);
title('Effect of R_4 value on output voltage');
xlabel('Time [s]');
ylabel('Voltage [V]');
legend('v_{in}', ...
'v_{out} (R_4 = 12k)', ...
'v_{out} (R_4 = 15k)', ...
'v_{out} (R_4 = 18k)', ...
'v_{out} (R_4 = 22k)', ...
'v_{out} (R_4 = 27k)' ...
);
% save high quality figure
% EPS figures are vector graphics, never get blurry
saveas(gcf,'week_2.eps')
% PNG are normal figures, uses lossless compression
% will get blurry if zoom in
saveas(gcf,'week_2.png')
% NEVER USE JPEGS FOR PLOTS!!
% Is lossy compression format
% High frequencies lost, which is needed for plots
% JPEG is acceptable for natural images (ie. photos etc)
% Try it yourself and compare the results, it will look bad :(
% saveas(gcf,'week_2.jpg')
