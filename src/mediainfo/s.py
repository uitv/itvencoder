import glob
import os

files = glob.glob ("desc_*")

for file in files:
    cml = "sed 's/<bitstream\/common.h>/\"common.h\"/g' %s > xxx/%s" % (file, file) 
    print cml
    os.popen (cml)
