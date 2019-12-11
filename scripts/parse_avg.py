import sys
import os
import numpy as np
import re
import pandas as pd

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

def get_avgs(path, regx_pattern):
    data = {"Type": [], "Workload": [], "Avg": []}
    
    for fsystem in os.listdir(path):
        
        for op_type in os.listdir(path + fsystem + "/"):
            filepath = path + "/" + fsystem + "/" + op_type + "/" + "filebench.out"
            
            if not os.path.isfile(filepath):
                print("File path does not exist: " + filepath)
                sys.exit()
            else:
                avg = get_avg(op_type, filepath, regx_pattern)
                data["Type"].append(fsystem)
                data["Workload"].append(op_type)
                data["Avg"].append(avg)

    return pd.DataFrame(data)

def calculate_overhead(baseline, comparison):
    baseline = baseline
    comparison = comparison
    overhead = baseline / comparison
    overhead = (1 - overhead) * 100
    return overhead

def get_default_fuse_overhead(dataframe):
   ext4_hdd = dataframe.loc[dataframe["Type"] == "HDD-EXT4-Results"].reset_index(drop=True) 
   fuse_hdd = dataframe.loc[dataframe["Type"] == "HDD-FUSE-EXT4-Results"].reset_index(drop=True)
   fuseopts_hdd = dataframe.loc[dataframe["Type"] == "HDD-FUSE-OPTS-EXT4-Results"].reset_index(drop=True)
   ext4_ssd = dataframe.loc[dataframe["Type"] == "SSD-EXT4-Results"].reset_index(drop=True)
   fuse_ssd = dataframe.loc[dataframe["Type"] == "SSD-FUSE-EXT4-Results"].reset_index(drop=True)
   fuseopts_ssd = dataframe.loc[dataframe["Type"] == "SSD-FUSE-OPTS-EXT4-Results"].reset_index(drop=True)

   ext4_hdd["Overhead"] = 0
   fuse_hdd["Overhead"] = calculate_overhead(ext4_hdd["Avg"], fuse_hdd["Avg"]) 
   fuseopts_hdd["Overhead"] = calculate_overhead(ext4_hdd["Avg"], fuseopts_hdd["Avg"])
   ext4_ssd["Overhead"] = 0 
   fuse_ssd["Overhead"] = calculate_overhead(ext4_ssd["Avg"], fuse_ssd["Avg"])
   fuseopts_ssd["Overhead"] = calculate_overhead(ext4_ssd["Avg"], fuseopts_ssd["Avg"])
   
   new_dataframe = ext4_ssd
   new_dataframe = new_dataframe.append(fuse_ssd, ignore_index=True)
   new_dataframe = new_dataframe.append(fuseopts_ssd, ignore_index=True)
   new_dataframe = new_dataframe.append(ext4_hdd, ignore_index=True)
   new_dataframe = new_dataframe.append(fuse_hdd, ignore_index=True)
   new_dataframe = new_dataframe.append(fuseopts_hdd, ignore_index=True)
   return new_dataframe.sort_values(by=["Type", "Workload"])

if __name__ == '__main__':
    regx_pattern = "[0-9]*ops\/s"
    path = sys.argv[1]
    parsed_avgs = get_avgs(path, regx_pattern)
    overheads = get_default_fuse_overhead(parsed_avgs) 
    print(overheads.to_string())
    #with pd.option_context('display.max_rows', None, 'display.max_columns', None):  # more options can be specified also
    #    print(overheads)

