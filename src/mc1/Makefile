# Builds Heretic2R as one static executable.
#
#   make TARGET=native    32-bit x86 Linux build (zig cc) for testing on a PC
#   make TARGET=mc1       MRISC32 build for the MC1 computer
#   make TARGET=sim       MRISC32 build for the mr32sim simulator
#
# Each game library (H2Common, Player, Client Effects, game) is compiled with hidden visibility and
# partially linked into one object, so only its dllexport symbols stay global. The exports of
# Player, Client Effects and game are then renamed with a module prefix, and modules_gen.c maps
# the original names back for LoadLibrary/GetProcAddress (see modules.c).

TARGET ?= native
SRC := ..
B := ../../build/$(TARGET)

ifeq ($(TARGET),native)
  CC := python3 -m ziglang cc -target x86-linux-musl
  CXX := python3 -m ziglang c++ -target x86-linux-musl
  LDR := ld -m elf_i386 -r -d
  OBJCOPY := objcopy
  NM := nm
  ARCHFLAGS := -O2 -g -fno-strict-aliasing
  LDLIBS := -lm
else
  CC := mrisc32-elf-gcc
  CXX := mrisc32-elf-g++
  LDR := mrisc32-elf-ld -r -d
  OBJCOPY := mrisc32-elf-objcopy
  NM := mrisc32-elf-nm
  ARCHFLAGS := -O2 -g -fno-strict-aliasing -ffast-math -DMC1
  ifeq ($(TARGET),sim)
    ARCHFLAGS += -DMR32SIM
    LDLIBS := -msim -lm
  else
    LDLIBS := -lmc1 -mno-crt0 -lmc1crt0-app -lm
  endif
endif

CFLAGS := -std=gnu17 $(ARCHFLAGS) -fvisibility=hidden -DNDEBUG -include $(SRC)/mc1/h2compat.h \
	-I$(SRC)/mc1/include -Wno-unknown-pragmas -Wno-pragma-once-outside-header -Wno-attributes \
	-Wno-unused-value -Wno-microsoft-anon-tag -Wno-incompatible-pointer-types-discards-qualifiers \
	-Wno-incompatible-function-pointer-types -Wno-incompatible-pointer-types -Wno-int-conversion \
	-Wno-unknown-warning-option -fms-extensions -fcommon
CXXFLAGS := $(filter-out -std=gnu17 -Wno-incompatible-pointer-types-discards-qualifiers,$(CFLAGS)) -std=gnu++17

find = $(patsubst $(SRC)/%,%,$(shell find $(SRC)/$(1) \( -name '*.c' -o -name '*.cpp' \) | sort))

H2COMMON_SRC := $(call find,H2Common/src) game/src/q_Shared.c mc1/h2common_init.c
H2COMMON_FLAGS := -DH2COMMON -I$(SRC)/H2Common/src -I$(SRC)/qcommon -I$(SRC)/game/src -I$(SRC)/Player/src

PLAYER_SRC := $(call find,Player/src) game/src/q_Shared.c
PLAYER_FLAGS := -DPLAYER_DLL -I$(SRC)/Player/src -I$(SRC)/game/src -I$(SRC)/qcommon

CLFX_SRC := $(call find,mc1/clfx/src/) qcommon/anorms.c qcommon/Message.c qcommon/netmsg_read.c qcommon/turbsin.c
CLFX_FLAGS := -I$(SRC)/mc1/clfx/src -I$(SRC)/Player/src -I$(SRC)/quake2/src -I$(SRC)/qcommon -I$(SRC)/game/src

GAME_SRC := $(call find,game/src) qcommon/Message.c qcommon/p_dll.c qcommon/Skeletons.c
GAME_FLAGS := -DGAME_DLL -I$(SRC)/game/src -I$(SRC)/Player/src -I$(SRC)/qcommon

ENGINE_PLATFORM := quake2/src/win32/sys_win.c quake2/src/win32/q_shwin.c quake2/src/win32/net_wins.c \
	quake2/src/client/glimp_sdl3.c quake2/src/client/input_sdl3.c
ENGINE_SRC := $(filter-out $(ENGINE_PLATFORM),$(call find,quake2/src)) \
	../include/libsmacker/smacker.c game/src/q_Shared.c \
	qcommon/anorms.c qcommon/netmsg_read.c qcommon/netmsg_write.c qcommon/p_dll.c qcommon/Reference.c \
	qcommon/Skeletons.c qcommon/turbsin.c \
	$(call find,mc1/platform) $(call find,ref_soft)
ENGINE_FLAGS := -DQUAKE2_DLL -I$(SRC)/quake2/src -I$(SRC)/../include -I$(SRC)/quake2/src/client \
	-I$(SRC)/quake2/src/server -I$(SRC)/qcommon -I$(SRC)/game/src -I$(SRC)/Player/src -I$(SRC)/ref_soft/src -I$(SRC)/mc1/platform \
	-I$(SRC)/quake2/src/win32/dll_io

obj = $(patsubst %,$(B)/$(1)/%.o,$(2))

all: $(B)/heretic2

define compile_rules
$(B)/$(1)/%.c.o: $(SRC)/%.c
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) $$($(1)_FLAGS) -MMD -c "$$<" -o $$@

$(B)/$(1)/%.cpp.o: $(SRC)/%.cpp
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXFLAGS) $$($(1)_FLAGS) -MMD -c "$$<" -o $$@

-include $$(patsubst %.o,%.d,$$(call obj,$(1),$$($(1)_SRC)))
endef

$(foreach m,H2COMMON PLAYER CLFX GAME ENGINE,$(eval $(call compile_rules,$(m))))

# Partial link, then hide everything that the module didn't export.
$(B)/h2common.o: $(call obj,H2COMMON,$(H2COMMON_SRC))
	$(LDR) -o $@.tmp $^
	$(OBJCOPY) --localize-hidden $@.tmp $@

# As above, then prefix the exports with the module name.
define module_rule
$(B)/$(1).o: $(call obj,$(2),$($(2)_SRC))
	$(LDR) -o $$@.tmp $$^
	$(OBJCOPY) --localize-hidden $$@.tmp $$@.tmp2
	$(NM) -g --defined-only $$@.tmp2 | awk '{ print $$$$3 " $(1)__" $$$$3 }' > $$@.syms
	$(OBJCOPY) --redefine-syms=$$@.syms $$@.tmp2 $$@
endef

$(eval $(call module_rule,player,PLAYER))
$(eval $(call module_rule,clfx,CLFX))
$(eval $(call module_rule,game,GAME))

$(B)/modules_gen.c: $(B)/player.o $(B)/clfx.o $(B)/game.o $(SRC)/mc1/genmodules.sh
	sh $(SRC)/mc1/genmodules.sh $(B)/player.o.syms:Player $(B)/clfx.o.syms:"Client Effects" \
		$(B)/game.o.syms:gamex86 > $@

$(B)/modules_gen.o: $(B)/modules_gen.c
	$(CC) $(CFLAGS) -I$(SRC)/mc1/platform -c $< -o $@

$(B)/heretic2: $(call obj,ENGINE,$(ENGINE_SRC)) $(B)/h2common.o $(B)/player.o $(B)/clfx.o $(B)/game.o \
		$(B)/modules_gen.o
	$(CXX) $(ARCHFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -rf $(B)

.PHONY: all clean
