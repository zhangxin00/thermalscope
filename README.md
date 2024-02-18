# ThermalScope

This repository contains the experiments of evaluation and case studies discussed in the paper  
* "ThermalScope: A Practical Interrupt Side Channel Attack Based On Thermal Event Interrupts" (DAC 2024).
  
ThermalScope distinguishs a new interrupt side channel. Our key observation is that workloads running on CPUs inevitably generates their distinct heat, which can be correlated with the thermal event interrupts.

## Tested Setup
* Linux installation
  * Build tools (gcc, make)

Throughout our experiments, we successfully evaluated our implementations on the following environments. We recommend to test SegScope on bare-metal machines.

| Machine                | CPU                  | Kernel          |
| ---------------------- | -------------------  | --------------- |
| Xiaomi Air 13.3        | Intel Core i5-8250U  | Linux 5.15.0    |
| Lenovo Savior R9000    | Intel Core i7-9750H  | Linux 5.8.0     |
| Gigabyte z790 (motherboard) | Intel Core i7-14700K | Linux 5.15.0 |

## Contact

If there are questions regarding these experiments, please send an email to `zhangxin00@stu.pku.edu.cn`. 
## How should I cite this work?

Please use the following BibTeX entry:

```latex
@inproceedings{Zhang2024ThermalScope,
  year={2024},
  title={ThermalScope: A Practical Interrupt Side Channel Attack Based On Thermal Event Interrupts},
  booktitle={Design Automation Conference},
  author={Xin Zhang and Zhi Zhang and Qingni Shen and Wenhao Wang and Yansong Gao and Zhuoxi Yang and Zhonghai Wu}
}
