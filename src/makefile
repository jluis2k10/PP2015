#-include ../makefile.init

RM := rm -rf
LIBS := -lmpi

# All of the sources participating in the build are defined here
#-include subdir.mk

################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
interactivo.c \
juegomus.c \
jugador.c \
main.c \
probsmus.c \
estructuras.c

OBJS += \
./src/interactivo.o \
./src/juegomus.o \
./src/main.o \
./src/probsmus.o \
./src/estructuras.o

OBJS_2 += \
./src/interactivo.o \
./src/juegomus.o \
./src/jugador.o \
./src/probsmus.o \
./src/estructuras.o  

C_DEPS += \
./src/interactivo.d \
./src/juegomus.d \
./src/jugador.d \
./src/main.d \
./src/probsmus.d \
./src/estructuras.d


# Each subdirectory must supply rules for building sources it contributes
src/%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	mpicc -I/usr/include/mpi -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '




ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

# All Target
all: Crear_src Mus Jugador Borrar

# Tool invocations
Crear_src:
	@echo 'Creando directorio ./src'
	mkdir ./src
	
Mus: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	mpicc -L/usr/include/lib -o "Mus" $(OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

Jugador: $(OBJS_2) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	mpicc -o "Jugador" $(OBJS_2) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	
Borrar:
	@echo 'Borrando dependencias e intermedios'
	-$(RM) $(OBJS) $(OBJS_2) $(C_DEPS)
	rmdir ./src
	

# Other Targets
clean:
	-$(RM) $(EXECUTABLES) Mus
	-$(RM) $(EXECUTABLES) Jugador
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
