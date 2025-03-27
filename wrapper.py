# Importing the NumPy library
import numpy as np
import subprocess
import argparse
import os

directory = os.fsencode("data/")

# Setup input arguments like IP and Port
parser = argparse.ArgumentParser(description='Automation Script for MLO')
parser.add_argument('-i', action="store", dest='ip', default='127.0.0.1')
parser.add_argument('-p', action="store", dest='port', default='3000')

# Make sure build is on latest ver
subprocess.run(["make", "clean"])
subprocess.run("make")

# Parse Algos
args = parser.parse_args()

# Loop over data and run the process
for file in os.listdir(directory):
    filename = os.fsdecode(file)
    temp_time = subprocess.run(["./route", '-c', '-i', args.ip, '-p', args.port,
        '-f', ("data/" + filename)], capture_output = True, text = True)
    print(temp_time)