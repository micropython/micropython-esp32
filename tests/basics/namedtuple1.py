try:
    try:
        from collections import namedtuple
    except ImportError:
        from ucollections import namedtuple
except ImportError:
    import sys
    print("SKIP")
    sys.exit()

T = namedtuple("Tup", ["foo", "bar"])
# CPython prints fully qualified name, what we don't bother to do so far
#print(T)
for t in T(1, 2), T(bar=1, foo=2):
    print(t)
    print(t[0], t[1])
    print(t.foo, t.bar)

    print(len(t))
    print(bool(t))
    print(t + t)
    print(t * 3)

    print([f for f in t])

    print(isinstance(t, tuple))

try:
    t[0] = 200
except TypeError:
    print("TypeError")
try:
    t.bar = 200
except AttributeError:
    print("AttributeError")

try:
    t = T(1)
except TypeError:
    print("TypeError")

try:
    t = T(1, 2, 3)
except TypeError:
    print("TypeError")

try:
    t = T(foo=1)
except TypeError:
    print("TypeError")

try:
    t = T(1, foo=1)
except TypeError:
    print("TypeError")

# enough args, but kw is wrong
try:
    t = T(1, baz=3)
except TypeError:
    print("TypeError")

# bad argument for member spec
try:
    namedtuple('T', 1)
except TypeError:
    print("TypeError")

# Try single string
T3 = namedtuple("TupComma", "foo bar")
t = T3(1, 2)
print(t.foo, t.bar)

# Try tuple
T4 = namedtuple("TupTuple", ("foo", "bar"))
t = T4(1, 2)
print(t.foo, t.bar)

# Try single string with comma field seperator
# Not implemented so far
#T2 = namedtuple("TupComma", "foo,bar")
#t = T2(1, 2)
