import colors
import framebuf
from framebuf import FrameBuffer
from array import array


class Area(object):

    def __init__(self, display, cols, rows, fg=colors.WHITE, bg=colors.BLACK,
                 border=colors.RED, padding=4, x=0, y=0):
        self.buffer = None
        self.fb = None
        self.display = display
        self.padding = padding
        self.bg = bg
        self.fg = fg
        self.border = border
        self.move(x, y)
        self.resize(cols, rows)
        if hasattr(self, 'init'):
            self.init()

    def init_fb(self):
        if self.buffer is None:
            self.buffer = bytearray(self.width * self.height * 2)
        if self.fb is None:
            self.fb = FrameBuffer(self.buffer, self.width, self.height,
                                  framebuf.RGB565)
        return self.fb

    def free_fb(self):
        self.buffer = None
        self.fb = None

    def move(self, x, y):
        self.x = x
        self.y = y

    def resize(self, cols, rows):
        pad = 1 + self.padding * 2
        self.rows = rows
        self.cols = cols
        self.width = cols * 8 + pad
        self.height = (rows * 8) + pad
        self.dirty = True

    def paint(self):
        fb = self.init_fb()
        fb.fill(self.bg)
        fb.fill_rect(0, 0, self.width, self.height, self.border)

    def blit(self):
        self.fb = None
        self.display.blit_buffer(
            self.buffer, self.x, self.y, self.width, self.height)
        self.buffer = None


class Point(object):
    x = 0
    y = 0
    v = 0

    def __init__(self, val):
        self.v = val


class Graph(Area):

    def init(self):
        self.scale_x = 8
        self.scale_y = 8
        self.points = array('B', [])

    def point(self, v):
        content_width = len(self.points) * self.scale_x
        window_width = self.width - 2 - self.padding * 2
        if (content_width >= window_width):
            self.points = self.points[1:]

        if v > self.scale_y:
            self.scale_y = int(v) + 1

        self.points.append(v)
        self.dirty = True

    def paint(self):
        if not self.dirty:
            return
        super(Graph, self).paint()
        prev = None
        pad = 1 + self.padding
        x = 0
        for v in self.points:
            point = Point(v)
            point.x = pad + x * self.scale_x
            v = point.v if point.v >= 1 else 1
            point.y = (self.height / self.scale_y) * (self.scale_y / v)
            point.y = round(self.height - point.y)
            point.y += self.padding + 1
            x += 1
            if (prev):
                self.fb.line(prev.x, prev.y, point.x, point.y, self.fg)
            prev = point

        self.blit()
        self.dirty = False


class TextArea(Area):

    def init(self):
        self.lines = []

    def text(self, txt):
        self.lines = txt.splitlines()
        self.dirty = True
        self.paint()

    def append(self, text):
        self.dirty = True
        if isinstance(text, str):
            lines = text.splitlines()
        elif isinstance(text, list):
            lines = text
        else:
            lines = str(text).splitlines()

        for line in lines:
            self.lines.append(line)
        scrollback = self.rows * 2
        if len(self.lines) > scrollback:
            self.lines = self.lines[-scrollback:]

    def paint(self, skip_lines=None):
        if not self.dirty:
            return

        super(TextArea, self).paint()

        pad = 1 + self.padding
        y = pad

        lines = self.lines
        if skip_lines:
            lines = lines[skip_lines - 1:]

        if len(lines) > self.rows:
            lines = lines[-self.rows:]

        for line in lines:
            if len(line) > self.cols:
                line = line[0:self.cols - 1]
            self.fb.text(line, pad, y, self.fg)
            y += 8
        self.blit()
        self.dirty = False
