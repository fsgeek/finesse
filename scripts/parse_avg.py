import sys
import os
import numpy as np
import re
import pandas as pd
import glob
import pdb

op_types = [
    "Stat-files-cr-wr-4KB-1th-4Mf",
    "Stat-files-cr-wr-4KB-32th-4Mf",
    "Stat-files-preall-de-4KB-1th-4Mf",
    "Stat-files-preall-de-4KB-32th-4Mf",
    "Stat-files-preall-re-4KB-1th-1Mf",
    "Stat-files-preall-re-4KB-32th-1Mf",
    "Stat-files-rd-re-1024KB-1th-1f",
    "Stat-files-rd-re-1024KB-32th-1f",
    "Stat-files-rd-re-128KB-1th-1f",
    "Stat-files-rd-re-128KB-32th-1f",
    "Stat-files-rd-re-32KB-1th-1f",
    "Stat-files-rd-re-32KB-32th-1f",
    "Stat-files-rd-re-4KB-1th-1f",
    "Stat-files-rd-re-4KB-32th-1f",
    "Stat-files-rd-wr-1024KB-1th-1f",
    "Stat-files-rd-wr-1024KB-32th-1f",
    "Stat-files-rd-wr-128KB-1th-1f",
    "Stat-files-rd-wr-128KB-32th-1f",
    "Stat-files-rd-wr-32KB-1th-1f",
    "Stat-files-rd-wr-32KB-32th-1f-",
    "Stat-files-rd-wr-4KB-1th-1f",
    "Stat-files-rd-wr-4KB-32th-1f",
    "Stat-files-sq-re-1024KB-1th-1f",
    "Stat-files-sq-re-1024KB-32th-1f",
    "Stat-files-sq-re-1024KB-32th-32f",
    "Stat-files-sq-re-128KB-1th-1f",
    "Stat-files-sq-re-128KB-32th-1f",
    "Stat-files-sq-re-128KB-32th-32f",
    "Stat-files-sq-re-32KB-1th-1f",
    "Stat-files-sq-re-32KB-32th-1f",
    "Stat-files-sq-re-32KB-32th-32f",
    "Stat-files-sq-re-4KB-1th-1f",
    "Stat-files-sq-re-4KB-32th-1f",
    "Stat-files-sq-re-4KB-32th-32f",
    "Stat-files-sq-wr-1024KB-1th-1f",
    "Stat-files-sq-wr-1024KB-32th-32f",
    "Stat-files-sq-wr-128KB-1th-1f",
    "Stat-files-sq-wr-128KB-32th-32f",
    "Stat-files-sq-wr-32KB-1th-1f",
    "Stat-files-sq-wr-32KB-32th-32f",
    "Stat-files-sq-wr-4KB-1th-1f",
    "Stat-files-sq-wr-4KB-32th-32f"
]

def parse_write(filepath, regx_pattern):
    with open(filepath) as fp:
        vals = np.array([])
        
        for line in fp:
            
            if "write-file" in line:
                opspersec = re.search(regx_pattern, line).group()
                parsedops = opspersec.split("o")[0]
                vals = np.append(vals, float(parsedops))
        
        return np.mean(vals)

def parse_read(filepath, regx_pattern):
    with open(filepath) as fp:

        vals = np.array([])
        for line in fp:
            
            if "read-file" in line:
                opspersec = re.search(regx_pattern, line).group()
                parsedops = opspersec.split("o")[0]
                vals = np.append(vals, float(parsedops))
        
        return np.mean(vals)


def parse_create(filepath, regx_pattern):
    with open(filepath) as fp:

        vals = np.array([])
        for line in fp:
            
            if "create1" in line:
                opspersec = re.search(regx_pattern, line).group()
                parsedops = opspersec.split("o")[0]
                vals = np.append(vals, float(parsedops))
        
        return np.mean(vals)

def parse_delete(filepath, regx_pattern):
    with open(filepath) as fp:

        vals = np.array([])
        for line in fp:
            
            if "delete-file" in line:
                opspersec = re.search(regx_pattern, line).group()
                parsedops = opspersec.split("o")[0]
                vals = np.append(vals, float(parsedops))
        
        return np.mean(vals)

def calculate_overhead(baseline, comparison):
    baseline = baseline
    comparison = comparison
    overhead = baseline / comparison
    overhead = (1 - overhead) * 100
    return overhead

def get_avg(optype, filepath, regx_pattern):
    avg = None
    if "-wr-" in optype:
        avg = parse_write(filepath, regx_pattern)
    elif "-re-" in optype:
        avg = parse_read(filepath, regx_pattern)
    elif "-cr-" in optype:
        avg = parse_create(filepath, regx_pattern)
    elif "-de-" in optype:
        avg = parse_delete(filepath, regx_pattern)
    else:
        print("Unknown parentdir: " + optype)

    return avg

def get_avgs(parentdir, fsystem_type, regx_pattern):
    dataframe = []
    if not os.path.isdir(parentdir + fsystem_type):
        print("There are no results in this directory: " + parentdir + fsystem_type)
    
    for op_type in op_types:
        data = {"Type": None, "Workload": None, "Avg": None, "Std": None, "Runs": None, "Overhead": None}
        
        file_paths = glob.glob(parentdir + fsystem_type + "/" + op_type + "*")
        
        if len(file_paths) == 0:
            print("Missing the results for " + fsystem_type + "/" + op_type)

        avgs = np.array([])
        for path in file_paths:
            avg = get_avg(op_type, path + "/filebench.out", regx_pattern)
            avgs = np.append(avgs, avg)
        
        data["Type"] = fsystem_type
        data["Workload"] = op_type
        data["Avg"] = np.mean(avgs)
        data["Std"] = np.std(avgs)
        data["Runs"] = len(avgs)
        dataframe.append(data)
    
    return pd.DataFrame(dataframe)

def parse_results(parentdir, regx_pattern):
    hdd_ext4 = get_avgs(parentdir, "HDD-EXT4-Results", regx_pattern)
    hdd_fuse = get_avgs(parentdir, "HDD-FUSE-EXT4-Results", regx_pattern)
    hdd_fuse_opts = get_avgs(parentdir, "HDD-FUSE-OPTS-EXT4-Results", regx_pattern)
    ssd_ext4 = get_avgs(parentdir, "SSD-EXT4-Results", regx_pattern)
    ssd_fuse = get_avgs(parentdir, "SSD-FUSE-EXT4-Results", regx_pattern)
    ssd_fuse_opts = get_avgs(parentdir, "SSD-FUSE-OPTS-EXT4-Results", regx_pattern)
    
    hdd_ext4["Overhead"] = 0
    hdd_fuse["Overhead"] = calculate_overhead(hdd_ext4["Avg"], hdd_fuse["Avg"]) 
    hdd_fuse_opts["Overhead"] = calculate_overhead(hdd_ext4["Avg"], hdd_fuse_opts["Avg"])
    ssd_ext4["Overhead"] = 0 
    ssd_fuse["Overhead"] = calculate_overhead(ssd_ext4["Avg"], ssd_fuse["Avg"])
    ssd_fuse_opts["Overhead"] = calculate_overhead(ssd_ext4["Avg"], ssd_fuse_opts["Avg"])

if __name__ == '__main__':
    regx_pattern = "[0-9]*ops\/s"
    path = sys.argv[1]
    parse_results(path, regx_pattern)
