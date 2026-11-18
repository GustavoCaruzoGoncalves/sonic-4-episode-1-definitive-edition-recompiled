import os

base = 'C:/S4EP1Recomp/pilot/generated/default'
found = False
for fn in os.listdir(base):
    if fn.endswith('.cpp'):
        path = os.path.join(base, fn)
        with open(path, encoding='utf-8', errors='ignore') as f:
            data = f.read()
        # look for function definition
        marker = 'sub_8220FD18'
        idx = data.find(marker)
        if idx >= 0:
            # check if it's a definition: void sub_... ( ... ) { 
            # look backwards a bit
            start_search = max(0, idx - 100)
            snippet = data[start_search:idx+200]
            # definition pattern
            if 'void ' + marker in snippet or marker in snippet.splitlines()[0] if '\n' in snippet[:50] else False:
                pass  # rough
            # more precise: search for the full def
            if 'void ' + marker in data:
                # find line
                for i, l in enumerate(data.splitlines(), 1):
                    if 'void ' + marker in l:
                        print(f'FOUND DEF in {fn}:{i}:{l}')
                        found = True
                        break
                if found:
                    break
            else:
                # maybe different format
                pass

if not found:
    print('not found')
