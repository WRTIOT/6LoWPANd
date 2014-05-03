############################################################################
#
# This software is owned by NXP B.V. and/or its supplier and is protected
# under applicable copyright laws. All rights are reserved. We grant You,
# and any third parties, a license to use this software solely and
# exclusively on NXP products [NXP Microcontrollers such as JN5148, JN5142, JN5139]. 
# You, and any third parties must reproduce the copyright and warranty notice
# and any other legend of ownership on each copy or partial copy of the 
# software.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Copyright NXP B.V. 2012. All rights reserved
#
############################################################################


FEATURES ?= 6LOWPAND_FEATURE_ZEROCONF

SOURCE := Serial.c SerialLink.c JennicModule.c TunDevice.c main.c

ifeq ($(findstring 6LOWPAND_FEATURE_ZEROCONF,$(FEATURES)),6LOWPAND_FEATURE_ZEROCONF)
SOURCE += Zeroconf.c
PROJ_CFLAGS += -DUSE_ZEROCONF
PROJ_LDFLAGS += -lavahi-client -lavahi-common -ldbus-1
endif

CFLAGS += -O2 -Wall -g

OBJ := $(SOURCE:.c=.o)

PROJ_CFLAGS += -I../Source/
PROJ_CFLAGS += -DVERSION="\"$(shell if [ -f version.txt ]; then cat version.txt; else svnversion ../Source; fi)\""

PROJ_LDFLAGS += -ldaemon -lpthread

vpath %.c ../Source

TARGET = 6LoWPANd

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC)  $^ $(LDFLAGS) $(PROJ_LDFLAGS) -o $@

%.o: %.c
	$(CC)  -I. $(CFLAGS) $(PROJ_CFLAGS) -c $<

install:
	mkdir -p $(DESTDIR)/sbin/
	cp $(TARGET) $(DESTDIR)/sbin/

clean:
	rm -f *.o $(TARGET)
