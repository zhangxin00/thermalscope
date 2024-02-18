#!/bin/bash

# Usage ./collect_traces.sh

##################### CLI ######################################

# Check CLI args
if [ $# -eq 0 ]; then
	echo -e "Usage: ./collect_traces.sh <Num of samples to collect per app> <Sampling Rate>\n"
	exit 1
fi

if [ "$1" == "-help" ]; then
	echo -e "Usage: ./collect_traces.sh <Num of samples to collect per program> <Sampling Rate> <sc/mc>\n"
	echo -e "	Num of samples to collect per app: The number of times to run each program\n"
	echo -e "	Sampling Rate: Sampling interval for tegrastats in ms\n"
	exit 1
fi

#sudo /usr/bin/jetson_clocks

# Print selected information
echo "Num of samples: $1"
echo "Sampling Rate: ${2}ms"

# List of programs to collect data from
declare -a programs=("squeezenet1_0" "squeezenet1_1" "shufflenetv2x05" "shufflenetv2x10" "resnet18" "resnet34" "resnet50" "resnet101" "resnet152" "densenet121" "densenet161" "densenet169" "densenet201" "vgg11" "vgg13" "vgg16" "vgg19" "alexnet" "mobilenetv2" "googlenet" "mnasnet0_5" "mnasnet0_75" "mnasnet1_0" "mnasnet1_3" "mobilenet_v3_large" "mobilenet_v3_small" "resnext50" "resnext101" "shufflenet_v2_15" "shufflenet_v2_20" "vgg11_bn" "vgg13_bn" "vgg16_bn" "vgg19_bn" "wide_resnet50" "wide_resnet101")


##################### Check files available ######################################

# Check if all files needed are available
echo -e "\nChecking for necessary files..."
cd ..
[ -d "imagenet" ] && echo "imagenet dataset directory found" || "Error: imagenet directory does not exist."

cd models
for i in "${dependancies[@]}"; do
	if [ -f "$PWD/$i.py" ]; then
		echo -e " $i.py \t\t\t FOUND"
	else
		echo -e "!! $i.py \t\t\t MISSING !!"
		exit 1
	fi
done
cd ../scripts

start_time=$(date +"%r")
echo "Start time : $start_time"

##################### Collect Traces ######################################

echo -e "\n#### Starting Data Collection ####\n"
sleep 2

# Number of samples (N) to collect per program
N=$1

# Sampling Rate (in ms)
freq=$2

# Command
cmd=(taskset -c 2 python3)
echo ${programs[@]}
mkdir raw_data

# Collect N traces from each program
for i in "${programs[@]}"; do
	# Command to run program
	run_cmd=${cmd[@]}
	run_cmd+=(../models/${i}.py)
	# Dir to store traces for a model
	mkdir ${i}
	#cd ${i}
	for j in $(seq 1 $N); do
		# See stats and progress
		echo -e "\n####################################################################"
		echo -e "####### ${i} | Run: ${j} / $N | Sampling Num: $freq #######"
		echo -e "####################################################################\n"
	
		# Start tegrastats monitoring
		#rm -rf temp
		./count3 ${freq} temp &
		
		./workload &
		# Run command/program
		${run_cmd[@]} ../imagenet/
		# End monitoring
		pkill -9 workload

		sleep 30
		mv temp ${i}_${freq}sample_${j}.txt 
		# Time to let commands finish and cleanup
		echo -e "\n"
	done

	# Move all traces to appropriate dir
	mv *.txt ${i}
	#cd ..
	mv ${i} raw_data
	unset run_cmd[-1]

done

sleep 1

##################### Finished ######################################

end_time=$(date +"%r")
echo "Start time : $start_time"
echo "End time : $end_time"
echo -e "\n########### ALL RUNS FINISHED ###########\n\n"
