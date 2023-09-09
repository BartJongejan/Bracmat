# launch.py - Python 3 stub loader, loads the 'prythat' module that was made by Cython.

# This code is always interpreted, like normal Python.
# It is not compiled to C.

import prythat

print("\nFirst show that Python's own eval function can sum the two floating point\nnumbers 1.9 and 2.1.\nHere is the answer:")
print(eval("1.9+2.1"))

print("\nInitialise Bracmat with the call 'prythat.init()'.")
prythat.init()

bracmatCode = "  put'\"Enter a Bracmat expression. (As a starter, you can enter a string.)\\n\"&get':?x&!x+Kater+Ambrosia+1+3"
print("\nThis Python script is now going to ask the 'prythat' module to evaluate a\nBracmat expression that first writes a prompt for input to the screen, then\nwaits for keyboard input, and finally returns the result of adding your input\nto the sum 'Kater+Ambrosia+1+3'. The Bracmat expression is this one:\n\n"+bracmatCode+"\n")
print("So here it is Bracmat interacting:\n")
answer = prythat.HolyGrail(bracmatCode)
print('The answer from Bracmat is:'+answer+'\n')
bracmatCode = "Ni$\"\na='ebbenhout'\nprint('a is now:'+a)\n\""
print("\nNow we ask the prythat module to ask Bracmat to ask Python to set the variable\n'a' to the string 'ebbenhout' and to print the value of that variable to the\nscreen, after the text 'a is now:'. The Bracmat expression that does this is\n\n"+bracmatCode+"\n")
answer = prythat.HolyGrail(bracmatCode)
print("\nThe expression returned from Bracmat in this case happens to be the value\nreturned by 'Ni$'. It is identical to the input to the 'Ni$' function, so 'Ni' is\nonly interesting for its side effects, such as setting a variable in Python.\nSo here is the answer:")
print(answer)
bracmatCode = "Ni!$\"3.9+7.123\":?x&out$(\"x is:\" !x)&all done"
print("\nNext, we ask the prythat module to ask Bracmat to make a call back to Python\nto add two floating point numbers, 3.9 and 7.123, using the functin 'Ni!$'.\nIn contrast to the 'Ni$' function, which is only used for its side effects,\nthe 'Ni!$' is called for the value it returns, in this case the result of\nsumming the floating point numbers. We ask Bracmat to output their sum after\nthe text 'x is:'")
answer = prythat.HolyGrail(bracmatCode)
print("This is the result:")
print(answer)
prythat.final()
print("\nWe have called prythat.final(), so now Bracmat is not available until we call\n'prythat.init()' again.")

