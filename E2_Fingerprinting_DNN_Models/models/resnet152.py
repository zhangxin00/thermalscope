import time
import os
#import copy
import sys
import torch
import torchvision
from torch.utils.data import DataLoader
from torchvision import transforms, datasets, models

def main(argv):
    #start = time.time()

    if len(sys.argv) < 2:
        print("Error: Missing argument - test dataset")
        exit(0)

    test_dir = sys.argv[1]
    
    batch_size = 32
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

    data_transform = transforms.Compose([
            transforms.Resize(224),
            transforms.ToTensor(),
            transforms.Normalize(mean=[0.485, 0.456, 0.406],
                                std=[0.229, 0.224, 0.225])
        ])

    #start1 = time.time()
    test_dataset = datasets.ImageFolder(root=test_dir,transform=data_transform)
    test_dataset_loader = torch.utils.data.DataLoader(test_dataset,batch_size=batch_size, shuffle=True)
    example = next(iter(test_dataset_loader))[0].to(device)
    #print('Loading data : %s seconds' % (time.time() - start1))

    #start2 = time.time()
    model = torchvision.models.resnet152(pretrained=True)
    model.eval()
    model.to(device)
    #print('Loading Model : %s seconds' % (time.time() - start2))

    #torch.cuda.synchronize()
    #start3 = time.time()
    start3 = time.time()
    while(time.time()-start3<20):
     with torch.no_grad():
         output = model(example)
     _, predicted = torch.max(output.data, 1)
    #print(predicted)

    #print('Total : %s seconds' % (time.time() - start))


if __name__ == "__main__":
    main(sys.argv[1:])
