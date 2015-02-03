# CompoundEyes

## Description of this branch

Two feature extractors are combined in this branch to conduct near-duplicate video detection tasks, which are:

1. Color distribution in the HSV color space.
2. Local binary pattern (LBP).

This combination also has been proved to have the highest accuracy in all the combinations of two extractors.

All the feature computations are parallelized in this branch.

These feature extractors run in parallel, whose computations of features could also take up one or more threads.

## Build the project

To build the project, the first thing to do is to install OpenCV on the computer, which could be as easy as entering:

~sudo ./opencv2\_4\_9.sh

This project was developed in the Eclipse IDE, so importing it into Eclipse finishes all the configurations. If something goes wrong or you want to build a new project, such configurations are indispensable:

1. Properties -> C/C++ Build -> Settings -> GCC C++ Compiler -> Includes -> Include paths (-l): Add /usr/local/include/opencv.
2. Properties -> C/C++ Build -> Settings -> GCC C++ Compiler -> Miscellaneous -> Other flags: Add -fopenmp.
3. Properties -> C/C++ Build -> Settings -> GCC C++ Linker -> Libraries -> Libraries (-l): Add opencv\_core gomp opencv\_imgproc opencv\_highgui opencv\_ml opencv\_video opencv\_features2d opencv\_calib3d opencv\_objdetect opencv\_contrib opencv\_legacy opencv\_flann.
4. Properties -> C/C++ Build -> Settings -> GCC C++ Linker -> Libraries -> Library search path (-L): Add /usr/local/lib.

Having been configured, build the project by Project -> Build All or Project -> Build Project in Eclipse. The makefile generated by Eclipse could be reused on other machines, by simply entering:
1. ~cd ./Debug
2. ~make

## Running the program

In the Eclipse IDE, such configurations are necessary for running the program:

1. Create a new entry in Run -> Run Configurations -> C/C++ Application
2. In Main -> C/C++ Application: entering (the location of the executable of the project), e.g., Debug/Hist\_Nest\_parallel\_2\_1.
3. In Arguments -> Program arguments: entering (the location of the training video set) (the location of the groundtruth file of the training videos) (the location of the testing video set) (the location of the groundtruth file of the testing videos), each argument occupies the whole line.
4. Before running the program, make sure that all the .conf files in ./src/Nest except nest.conf have already been deleted, otherwise, new configurations will not have effect on the program.

An example for step 3 is:

/home/yixin/dataset/Test_s_0.1_p_0.5/Test_v1/1_train/

/home/yixin/dataset/Test_s_0.1_p_0.5/Test_v1/groundtruth/GT_new_1_train.txt

/home/yixin/dataset/Test_s_0.1_p_0.5/Test_v1/1_test/

/home/yixin/dataset/Test_s_0.1_p_0.5/Test_v1/groundtruth/GT_new_1_test.txt.
