#! /usr/bin/env python

import os

cur_file = os.getcwd() + os.sep + 'scan.py'

def add_namespace(old):
    new = old + '.new'
    fd = open(old, 'r')
    lines = fd.readlines()
    fd.close()

    new_fd = open(new, 'w')
    for line in lines:
        if line == 'namespace boost {\n':
            new_fd.write('namespace sofa {\n')
            new_fd.write('namespace pbrpc {\n')
            new_fd.write(line)
            continue
        if line == '} // namespace boost\n' :
            new_fd.write(line)
            new_fd.write('} // namespace pbrpc\n')
            new_fd.write('} // namespace sofa\n')
            continue
        new_fd.write(line)
    new_fd.close()
    os.system('mv ' + new + ' ' + old);

def replace_using(old):
    fd = open(old, 'r')
    lines = fd.readlines()
    fd.close()

    new = old + '.new'
    new_fd = open(new, 'w')
    for line in lines:
        line = line.replace('boost::asio', 'sofa::pbrpc::boost::asio')
        new_fd.write(line)
    new_fd.close()
    os.system('mv ' + new + ' ' + old)

def replace_header(old):
    fd = open(old, 'r')
    lines = fd.readlines()
    fd.close()

    new = old + '.new'
    new_fd = open(new, 'w')
    for line in lines:
        line = line.replace('<boost/asio', '<asio')
        new_fd.write(line)
    new_fd.close()
    os.system('mv ' + new + ' ' + old)

def scandir(startdir) :
    os.chdir(startdir)
    for obj in os.listdir(os.curdir) :
        full_path = os.getcwd() + os.sep + obj
        if full_path == cur_file:
            continue
        if os.path.isfile(full_path):
            #add_namespace(full_path)
            replace_header(full_path)
            #replace_using(full_path)
            print full_path, ' Processed ...'
        if os.path.isdir(obj) :
            scandir(obj)
            os.chdir(os.pardir)

#startdir = raw_input('Please input startdir: ')
startdir = '../' 
scandir(startdir)
