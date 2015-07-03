#
#  Copyright (c) 2011 Arduino.  All right reserved.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

SUBMAKE_OPTIONS=--no-builtin-rules --no-builtin-variables --no-print-directory

#-------------------------------------------------------------------------------
# Rules
#-------------------------------------------------------------------------------

all: duet

.PHONY: duet
duet:
	@echo ------------------------------------------------------------------------------------
	@echo --- Making variant duet
#	@$(MAKE) DEBUG=1 $(SUBMAKE_OPTIONS) -f libvariant_duet.mk
	@$(MAKE) $(SUBMAKE_OPTIONS) -f libvariant_duet.mk
	@echo ------------------------------------------------------------------------------------

.PHONY: clean
clean:
	@echo ------------------------------------------------------------------------------------
	@echo --- Cleaning variant arduino_due_x
#	@$(MAKE) DEBUG=1 $(SUBMAKE_OPTIONS) -f libvariant_duet.mk $@
	@$(MAKE) $(SUBMAKE_OPTIONS) -f libvariant_duet.mk $@
	@echo ------------------------------------------------------------------------------------

