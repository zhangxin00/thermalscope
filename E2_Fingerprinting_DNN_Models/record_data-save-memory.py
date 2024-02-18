import os

if "DISPLAY" not in os.environ:
    # Start selenium on main display if starting experiment over SSH
    os.environ["DISPLAY"] = ":0"

from enum import Enum

import argparse
import ctypes
import json
import logging
import math
import os
import pickle
import queue
import requests
import shutil
import signal
import subprocess
import sys
import threading
import time
from ctypes import *
import os
from PIL import Image

from flask import Flask, send_from_directory
from tqdm import tqdm, trange
from urllib3.exceptions import MaxRetryError, ProtocolError

import pandas as pd

import torch
import torch.nn.functional as F
from torchvision import models, transforms

my_file_path='/home/xin/Interrupt-model-fingerprinting/'

parser = argparse.ArgumentParser(description='Automate the collection of browser-based CPU traces.')
parser.add_argument("--num_runs", type=int, default=10)
parser.add_argument("--attacker_type", type=str, choices=["counter","tick"], default="tick")
parser.add_argument("--trace_length", type=int, default=2, help="The length of each recorded trace, in seconds.")
parser.add_argument("--out_directory", type=str, default="data", help="The output directory name.")
parser.add_argument("--overwrite", type=bool, default=False, help="True if we want to overwrite the output directory.")

opts = parser.parse_args()

# Make sure existing processes aren't running
procs = subprocess.check_output(["ps", "aux"]).decode("utf-8").split("\n")

def confirm(prompt):
    response = input(f"{prompt} [y/N] ")

    if "y" not in response.lower():
        sys.exit(1)
        
for term in ["python", "chrome", "safaridriver"]:
    conflicts = []

    for p in procs:
        if len(p) < 2 or not p.split()[1].isnumeric() or os.getpid() == int(p.split()[1]):
            continue

        if term.lower() in p.lower():
            conflicts.append(p)
    
    if len(conflicts) > 0:
        print()
        print("Processes")
        print("=========")
        print("\n".join(conflicts))
        confirm(f"Potentially conflicting {term} processes are currently running. OK to continue?")

# Double check that we're not overwriting old data
if not opts.overwrite and os.path.exists(opts.out_directory):
    print(f"WARNING: Data already exists at {opts.out_directory}. What do you want to do?")
    res = input("[A]ppend [C]ancel [O]verwrite ").lower()

    if res == "a":
        pass
    elif res == "o":
        confirm(f"WARNING: You're about to overwrite {opts.out_directory}. Are you sure?")
        shutil.rmtree(opts.out_directory)
    else:
        sys.exit(1)
elif opts.overwrite:
    shutil.rmtree(opts.out_directory)

if not os.path.exists(opts.out_directory):
    os.mkdir(opts.out_directory)


#####################################################
#model preparation
print(torch.cuda.is_available())
model = models.resnet18(pretrained=True)
#####################################################
def create_browser():
    browser = get_driver(opts.browser)
    browser.set_page_load_timeout(opts.trace_length)
    return browser
    


transform = transforms.Compose([
    transforms.Resize(256),
    transforms.CenterCrop(224),
    transforms.ToTensor(),
    transforms.Normalize(
        mean=[0.485, 0.456, 0.406],
        std=[0.229, 0.224, 0.225]
    )
])

def preimg(img):
    if img.mode == 'RGBA':
        ch = 4
        a = np.asarray(img)[:, :, :3]
        img = Image.fromarray(a)
    return img
im = preimg(Image.open('panda.jpg'))
transformed_img = transform(im)
inputimg = transformed_img.unsqueeze(0)


    
def create_irq(domain):
    #global model
    model_eva = model.eval()
    for i in range(32):
        output = model_eva(inputimg)
        output = F.softmax(output, dim=1)
        prediction_score, pred_label_idx = torch.topk(output, 3)
        prediction_score = prediction_score.detach().numpy()[0]
    #print(prediction_score[0])

    
def get_time():
        return time.time()

def collect_data(q):
    data = [-1] * (opts.trace_length * 1000)
    trace_time = get_time() * 1000
    idx = 0
    if opts.attacker_type == "tick":
        #print("select attacker_type=tick")
        while True:
            count_full = cdll.LoadLibrary(my_file_path + '/full3.so')       
            if idx >= len(data):
                break
            data[idx] = count_full.count_tick()
            idx += 1

    

    if opts.attacker_type == "counter":
        data = [-1] * (opts.trace_length * 10000)
        trace_time = get_time() * 10000
        while True:
            datum_time = get_time() * 10000
            idx = math.floor(datum_time - trace_time)

            if idx >= len(data):
                break

            num = 0

            while get_time() * 10000 - datum_time < 5:
                num += 1

            data[idx] = num
            
    q.put(data)


def record_trace(domain):
    q = queue.Queue()
    thread = threading.Thread(target=collect_data, name="record", args=[q])
    thread.start()

    start_time = time.time()

    try:
        create_irq(domain)

        pass
    except (MaxRetryError, ProtocolError):
        # Usually called on CTRL+C
        return None
    except Exception as e:
        print(type(e).__name__, e)
        return None

    #sleep_time = opts.trace_length - (time.time() - start_time)

    #if sleep_time > 0:
       # time.sleep(sleep_time)
    
    thread.join()
    results = [q.get()]

    if len(results[0]) == 1 and results[0][0] == -1:
        return None

    return results


recording = True


def signal_handler(sig, frame):
    global recording
    recording = False
    print("Wrapping up...")


signal.signal(signal.SIGINT, signal_handler)




def should_skip(domain):
    out_f_path = os.path.join(opts.out_directory, f"{domain.replace('https://', '').replace('http://', '').replace('www.', '')}.pkl")

    if os.path.exists(out_f_path):
        f = open(out_f_path, "rb")
        num_runs = 0

        while True:
            try:
                pickle.load(f)
                num_runs += 1
            except:
                break
        
        if num_runs == opts.num_runs:
            return True

    return False


def run(domain, update_fn=None):
    global model
    i = 0
    out_f_path = os.path.join(opts.out_directory, f"{domain.replace('https://', '').replace('http://', '').replace('www.', '')}.pkl")
    out_f_path=my_file_path+out_f_path
    out_f = open(out_f_path, "wb")
    if(domain=="resnet18"):
        model = models.resnet18(pretrained=True)
    elif(domain=="resnet34"):
        model = models.resnet34(pretrained=True)
    elif(domain=="resnet50"):
        model = models.resnet50(pretrained=True)
    elif(domain=="resnet101"):
        model = models.resnet101(pretrained=True)
    elif(domain=="resnet152"):
        model = models.resnet152(pretrained=True)

    elif(domain=="vgg11"):
        model = models.vgg11(pretrained=True)
    elif(domain=="vgg13"):
        model = models.vgg13(pretrained=True)
    elif(domain=="vgg16"):
        model = models.vgg16(pretrained=True)
    elif(domain=="vgg19"):
        model = models.vgg19(pretrained=True)
    elif(domain=="vgg11_bn"):
        model = models.vgg11_bn(pretrained=True)
    elif(domain=="vgg13_bn"):
        model = models.vgg13_bn(pretrained=True)
    elif(domain=="vgg16_bn"):
        model = models.vgg16_bn(pretrained=True)
    elif(domain=="vgg19_bn"):
        model = models.vgg19_bn(pretrained=True)
        
    elif(domain=="shufflenet_v2_05"):
        model = models.shufflenet_v2_x0_5(pretrained=True)
    elif(domain=="shufflenet_v2_10"):
        model = models.shufflenet_v2_x1_0(pretrained=True)
    elif(domain=="shufflenet_v2_15"):
        model = models.shufflenet_v2_x1_5(pretrained=True)
    elif(domain=="shufflenet_v2_20"):
        model = models.shufflenet_v2_x2_0(pretrained=True)
        
    elif(domain=="inception_v3"):
        model = models.inception_v3(pretrained=True)
    
    elif(domain=="alexnet"):
        model = models.alexnet(pretrained=True)
    
    elif(domain=="mobilenet_v2"):
        model = models.mobilenet_v2(pretrained=True)
    elif(domain=="mobilenet_v3_small"):
        model = models.mobilenet_v3_small(pretrained=True)
    elif(domain=="mobilenet_v3_large"):
        model = models.mobilenet_v3_large(pretrained=True)

    elif(domain=="squeezenet10"):
        model = models.squeezenet1_0(pretrained=True)
    elif(domain=="squeezenet11"):
        model = models.squeezenet1_1(pretrained=True)
        
    elif(domain=="densenet121"):
        model = models.densenet121(pretrained=True)
    elif(domain=="densenet161"):
        model = models.densenet161(pretrained=True)
    elif(domain=="densenet169"):
        model = models.densenet169(pretrained=True)
    elif(domain=="densenet201"):
        model = models.densenet201(pretrained=True)   
         
    elif(domain=="resnext50"):
        model = models.resnext50_32x4d(pretrained=True)       
    elif(domain=="resnext101"):
        model = models.resnext101_32x8d(pretrained=True)           

    elif(domain=="mnasnet0_5"):
        model = models.mnasnet0_5(pretrained=True) 
    elif(domain=="mnasnet0_75"):
        model = models.mnasnet0_75(pretrained=True)
    elif(domain=="mnasnet1_0"):
        model = models.mnasnet1_0(pretrained=True) 
    elif(domain=="mnasnet1_3"):
        model = models.mnasnet1_3(pretrained=True)  
 
    elif(domain=="wide_resnet50"):
        model = models.wide_resnet50_2(pretrained=True)
    elif(domain=="wide_resnet101"):
        model = models.wide_resnet101_2(pretrained=True)

    elif(domain=="googlenet"):
        model = models.googlenet(pretrained=True)
    # Add one so that we can have a first run where the site gets cached.
    while i < opts.num_runs + 1:
        if not recording:
            break


        trace = record_trace(domain)

        if trace is None:
            out_f.close()
            return False

        if i > 0:
            # Don't save first run -- site needs to be cached.
            data = (trace, domain)

            # Save data to output file incrementally -- this allows us to save much
            # more data than fits in RAM.
            pickle.dump(data, out_f)

            if update_fn is not None:
                update_fn()


        i += 1
    
    out_f.close()
    return True

browser = None
domains=["resnet18","resnet34","resnet50","resnet101","resnet152","vgg11","vgg13","vgg16","vgg19","vgg11_bn",\
"vgg13_bn","vgg16_bn","vgg19_bn","shufflenet_v2_05","shufflenet_v2_10","shufflenet_v2_15","shufflenet_v2_20","googlenet","alexnet","mobilenet_v3_small",\
"mobilenet_v2","mobilenet_v3_large","squeezenet10","squeezenet11","densenet121","densenet161","densenet169","densenet201","mnasnet0_5","mnasnet0_75",\
"mnasnet1_0","mnasnet1_3","resnext101","resnext50","wide_resnet50","wide_resnet101"]
total_traces = opts.num_runs * len(domains)

with tqdm(total=total_traces) as pbar:

    traces_collected = 0

    def post_trace_collection():
        global traces_collected, notify_interval, last_notification

        pbar.update(1)
        traces_collected += 1


    for i, domain in enumerate(domains):
        if not recording:
            break
        elif should_skip(domain):
            continue

        

        success = run(domain, update_fn=post_trace_collection)

        if success:
            pbar.n = (i + 1) * opts.num_runs
            pbar.refresh()



