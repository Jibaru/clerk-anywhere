#---------------------------------------------------------------------------------------------------------------------
# Clerk Anywhere - Game Boy Advance demo (Butano)
# Built inside the Docker image from ./Dockerfile, where the project is mounted at /work and Butano
# lives in /opt/butano. Butano resolves these paths relative to the project, hence ../opt.
#---------------------------------------------------------------------------------------------------------------------
BUTANOROOT      ?=  ../opt/butano

TARGET          :=  clerk-anywhere-gba
BUILD           :=  build
LIBBUTANO       :=  $(BUTANOROOT)/butano
PYTHON          :=  python
SOURCES         :=  src third_party/qrcodegen $(BUTANOROOT)/common/src
INCLUDES        :=  include third_party/qrcodegen $(BUTANOROOT)/common/include
DATA            :=
GRAPHICS        :=  $(BUTANOROOT)/common/graphics
AUDIO           :=
AUDIOBACKEND    :=  null
AUDIOTOOL       :=
DMGAUDIO        :=
DMGAUDIOBACKEND :=  null
ROMTITLE        :=  CLERKANYWHER
ROMCODE         :=  SCAE
USERFLAGS       :=
USERCXXFLAGS    :=
USERASFLAGS     :=
USERLDFLAGS     :=
USERLIBDIRS     :=
USERLIBS        :=
DEFAULTLIBS     :=  true
STACKTRACE      :=
USERBUILD       :=
EXTTOOL         :=

ifndef LIBBUTANOABS
	export LIBBUTANOABS	:=	$(realpath $(LIBBUTANO))
endif

include $(LIBBUTANOABS)/butano.mak
