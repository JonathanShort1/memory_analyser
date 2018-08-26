import sys
import json 

def main(filename):
    with open(filename) as f:
        data = json.load(f)

    task_struct = data["all_vtypes"]["task_struct"]
    size = task_struct[0]

    print "size: %#d, %#x" % (size, size)
    print "pid offset: %#d, %#x" % (task_struct[1]["pid"][0], task_struct[1]["pid"][0])
    print "comm offset: %#d, %#x" % (task_struct[1]["comm"][0], task_struct[1]["comm"][0])
    
    



if __name__ == '__main__':
    filename = sys.argv[1]
    main(filename)