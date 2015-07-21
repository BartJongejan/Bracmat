# launch.py - Python stub loader, loads the 'brapy' module that was made by Cython.

# This code is always interpreted, like normal Python.
# It is not compiled to C.

import prythat

print eval("1+2")
prythat.init()

bracmatCode = "put'\"Enter a Bracmat expression. (As a starter, you can enter a string.)\\n\"&get':?x&!x+Kater+Ambrosia+1+3"
print "Going to evaluate \n\t"+bracmatCode
answer = prythat.HolyGrail(bracmatCode)
print 'Answer from Bracmat:'+answer
bracmatCode = "Ni$\"\na='ebbenhout'\nprint('a:'+str(a))\n\""
print "Going to evaluate \n\t"+bracmatCode
answer = prythat.HolyGrail(bracmatCode)
print 'Answer from Bracmat:'
print answer
bracmatCode = "Ni!$\"3+7\":?x&out$(\"x is:\" !x)"
answer = prythat.HolyGrail(bracmatCode)

prythat.final()

