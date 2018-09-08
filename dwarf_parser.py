import sys
import json 

def main(filename):
    with open(filename) as f:
        data = json.load(f)

    task_struct = data["all_vtypes"]["task_struct"]
    size = task_struct[0]

    print "size: %#x" % size
    print "pid offset: %#x" % task_struct[1]["pid"][0]
    print "ppid offset: %#x" % task_struct[1]["ppid"][0]
    print "comm offset: %#x" %  task_struct[1]["comm"][0]
    print "parent offset: %#x" % task_struct[1]["parent"][0]
    print "childlist off: %#x" % task_struct[1]["children"][0]
    print "task off: %#x" % task_struct[1]["tasks"][0]



if __name__ == '__main__':
    filename = sys.argv[1]
    main(filename)