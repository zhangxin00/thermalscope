import pandas as pd
import re
import os
import sys

def process_file(idx, label, df, in_file):
    with open(in_file) as f:
        lines = f.readlines()

    used_ram = []

    for line in lines:
        match = re.search(r'\d+', line).group()
        used_ram.append(int(match))
        #matches = re.findall('\d*\.?\d+', line)
        #used_ram.append(int(matches[0]))

    df.loc[idx] = [idx, used_ram, len(used_ram), label]


def main(argv):

    if len(sys.argv) < 3:
        print("Error: Missing argument(s)")
        print("Usage: %s <path_to_raw_data> <output.pkl>" % (sys.argv[0]))
        exit(0)
    
    raw_data = sys.argv[1]
    output = sys.argv[2]

    if not output.endswith('.pkl'):
        print('Error: output file must be of type \'.pkl\'')
        exit(0)

    feature_list = ['id', 'used_ram', 'len' , 'label']
    df = pd.DataFrame(columns=feature_list)
    models = len(os.listdir(raw_data))
    for i, model in enumerate(os.scandir(raw_data)):
        label = model.name
        N = len(os.listdir(model.path))
        #print(' %s [%d/%d]' % (label, i, models), end='')
        for j, file in enumerate(os.scandir(model.path)):
            idx = i * N + j
            #print(idx, '  ', label, '-- ', file.name, ': ', file.path)
            process_file(idx, label, df, file.path)
            sys.stdout.write('\r')
            sys.stdout.write('%s [%d/%d]' % (label, j, N-1))
            sys.stdout.flush()
        print()
    
    df.to_pickle(output,protocol=4)
    print('\nData saved in ', output)


if __name__ == '__main__':
    main(sys.argv[1:])
