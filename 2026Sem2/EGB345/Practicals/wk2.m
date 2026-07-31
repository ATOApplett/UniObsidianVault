%% File Info

% Code written by: Jason Ford

% Class: EGB345

% Subcomponent: Practical 1

% Version Control: v1.1 7/4/2025

  

  

% Activity 1.1

% Student activity: Extract data from the task sheet for variables L, C R1

% and R2.

% Student activity: uncomment next 4 lines, and change XX to correct values

  

L=0.001;

C=0.0001;

R2=1;

R1=0.05;

  

% help tf

  

Num=[1/(L*C)];

Dem=[1, 1/(R2*C)+R1/L, (R1+R2)/(R2*L*C)];

  

sys = tf(Num,Dem)

  

% note figure number convention, figure # indicates activity number

figure(112)

  

%% For R2023a or later releases of MATLAB, see https://au.mathworks.com/help/ident/ref/respconfig.html

% opt = RespConfig;

% opt.Amplitude = 2;

  

%% For R2021b release of MATLAB, see https://au.mathworks.com/help/control/ref/stepdataoptions.html?searchHighlight=stepDataOptions&s_tid=srchtitle_stepDataOptions_1

opt = stepDataOptions('StepAmplitude',12);

step(sys,opt)

  

% note this opt part of step command is different since R2023a of MATLAB

%

  

  

% Activity 1.2

  

%Student activity: read the help information for the load command

  

help load

  

%Student activity: modify file_name to the relevant file name

  

load('SEA_speed-1.mat')

figure(121)

plot(SEA_time,SEA_speed)

xlabel('Time (s)')

ylabel('Speed (rad/s)')

  

%Student activity: read the help information for the csv command

help csvread

  

%Student activity: modify file_name.csv to the relevant file name

%Student activity: try to understand the syntax of the command, look at the

%spreadsheet in another application

  

servo_data = csvread('SEA_speed-1.csv',2,0);

  

%Student activity: change "change_me" and "change_me2" to give you data

%variables meaningful name

second = servo_data(:,1);

volt = servo_data(:,2);

figure(122)

plot(second,volt)

xlabel('Time (s)')

ylabel('Speed (rad/s)')

  

  

%Student activity: change "data_time_csv", "data_speed_csv", "data_time"

%and "data_speed" to to data variable names you have used to save the speed

%and time information from above.

  

figure(123)

plot(SEA_time,SEA_speed,'b',second,volt,'r')

xlabel('Time (s)')

ylabel('Speed (rad/s)')