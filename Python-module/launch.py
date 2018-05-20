# launch.py - Python 3 stub loader, loads the 'prythat' module that was made by Cython.

# This code is always interpreted, like normal Python.
# It is not compiled to C.

import prythat

print("First show that Python's own eval function can sum the two floating point\nnumbers 1.9 and 2.1.")
print(eval("1.9+2.1"))

print("Initialise Bracmat with the call 'prythat.init()'.")
prythat.init()

bracmatCode = "put'\"Enter a Bracmat expression. (As a starter, you can enter a string.)\\n\"&get':?x&!x+Kater+Ambrosia+1+3"
print("Going to evaluate a Bracmat expression that first writes a prompt for input to\nthe screen, then waits for keyboard input, and finally returns the result of\nadding your input to the sum 'Kater+Ambrosia+1+3'. The expression is this one:\n\n"+bracmatCode+"\n")
answer = prythat.HolyGrail(bracmatCode)
print('Answer from Bracmat:'+answer+'\n')
bracmatCode = "Ni$\"\na='ebbenhout'\nprint('a is now:'+a)\n\""
print("Now we ask Bracmat to ask Python to first set the variable 'a' to the string\n'ebbenhout' and then print the value of that variable to the screen, after the\ntext 'a is now:'. The Bracmat expression that does this is\n\n"+bracmatCode+"\n")
answer = prythat.HolyGrail(bracmatCode)
print("The expression returned from Bracmat in this case happens to be the value\nreturned by 'Ni$', is identical to the input to the 'Ni$' function, so 'Ni' is\nonly interesting for its side effects, such as setting a variable in Python:")
print(answer)
bracmatCode = "Ni!$\"3.9+7.123\":?x&out$(\"x is:\" !x)&all done"
print("Next, we ask Bracmat to make a call back to Python to add two floating point\nnumbers, 3.9 and 7.123, using the functin 'Ni!$'. In contrast to the 'Ni$'\nfunction, which is only used for its side effects, in the case of 'Ni!$' it is\nabout the returned value, in this case the result of summing the floating point\nnumbers. We ask Bracmat to output their sum after the text 'x is:'")
answer = prythat.HolyGrail(bracmatCode)
print("This is what we got back from Bracmat:")
print(answer)
prythat.final()
print("We have called prythat.final(), so now Bracmat is not available until we call\n'prythat.init()' again.")

