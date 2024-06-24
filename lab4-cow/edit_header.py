#!/usr/bin/env python3

import os

os.system('ls kernel/*.h user/*.h > edit_header.log')
assert os.path.exists('./edit_header.log')

headers = []
with open('edit_header.log', 'r') as fobj:
    for header in fobj:
        headers.append(header[:-1])

def edit(header):
    if not os.path.exists(header):
        print(f"path {header} is not valid")
        return False

    lines = []
    with open(header, 'r') as fobj:
        for r in fobj:
            lines.append(r)

    it = len(header) - 1
    while header[it] != '/':
        it -= 1
    
    macro = header[it+1:-2]
    macro = macro.upper()
    macro += '_H\n'
    with open(header, 'w') as fobj:
        fobj.write("#pragma once\n")
        fobj.write("#ifndef ")
        fobj.write(macro)
        fobj.write("#define ")
        fobj.write(macro + '\n')
        for line in lines:
            fobj.write(line)
        
        fobj.write("\n#endif // " + macro)
    return True

for header in headers:
    edit(header)
    print("Done with " + header)
