/*
  Copyright (c) 2011 Arduino.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Arduino.h"

#ifdef __cplusplus
 extern "C" {
#endif

extern void pinModeDuet( uint32_t ulPin, uint32_t ulMode, uint32_t debounceCutoff )
{
	if ( ulPin > MaxPinNumber || g_APinDescription[ulPin].ulPinType == PIO_NOT_A_PIN )
    {
        return ;
    }

	switch ( ulMode )
    {
        case INPUT:
            /* Enable peripheral for clocking input */
            pmc_enable_periph_clk( g_APinDescription[ulPin].ulPeripheralId ) ;
            PIO_Configure(
            	g_APinDescription[ulPin].pPort,
            	PIO_INPUT,
            	g_APinDescription[ulPin].ulPin,
				(debounceCutoff == 0) ? 0 : PIO_DEBOUNCE );
            if (debounceCutoff != 0)
            {
            	PIO_SetDebounceFilter(g_APinDescription[ulPin].pPort, g_APinDescription[ulPin].ulPin, debounceCutoff);	// enable debounce filer with specified cutoff frequency
            }
        break ;

        case INPUT_PULLUP:
            /* Enable peripheral for clocking input */
            pmc_enable_periph_clk( g_APinDescription[ulPin].ulPeripheralId ) ;
            PIO_Configure(
            	g_APinDescription[ulPin].pPort,
            	PIO_INPUT,
            	g_APinDescription[ulPin].ulPin,
				(debounceCutoff == 0) ? PIO_PULLUP : PIO_PULLUP | PIO_DEBOUNCE );
            if (debounceCutoff != 0)
            {
            	PIO_SetDebounceFilter(g_APinDescription[ulPin].pPort, g_APinDescription[ulPin].ulPin, debounceCutoff);	// enable debounce filer with specified cutoff frequency
            }
        break ;

        case OUTPUT:
            PIO_Configure(
            	g_APinDescription[ulPin].pPort,
            	PIO_OUTPUT_1,
            	g_APinDescription[ulPin].ulPin,
            	g_APinDescription[ulPin].ulPinConfiguration ) ;

            /* if all pins are output, disable PIO Controller clocking, reduce power consumption */
            if ( g_APinDescription[ulPin].pPort->PIO_OSR == 0xffffffff )
            {
                pmc_disable_periph_clk( g_APinDescription[ulPin].ulPeripheralId ) ;
            }
        break ;

        default:
        break ;
    }
}

extern void pinMode( uint32_t ulPin, uint32_t ulVal )
{
	pinModeDuet(ulPin, ulVal, 0);
}

extern void digitalWrite( uint32_t ulPin, uint32_t ulVal )
{
  /* Handle */
  if ( ulPin > MaxPinNumber || g_APinDescription[ulPin].ulPinType == PIO_NOT_A_PIN )
  {
    return ;
  }

  if (ulVal)		// we make use of the fact that LOW is zero and HIGH is nonzero
  {
    g_APinDescription[ulPin].pPort->PIO_SODR = g_APinDescription[ulPin].ulPin;
  }
  else
  {
    g_APinDescription[ulPin].pPort->PIO_CODR = g_APinDescription[ulPin].ulPin;
  }
}

extern int digitalRead( uint32_t ulPin )
{
	if ( ulPin > MaxPinNumber || g_APinDescription[ulPin].ulPinType == PIO_NOT_A_PIN )
    {
        return LOW ;
    }

	return (PIO_Get(g_APinDescription[ulPin].pPort, PIO_INPUT, g_APinDescription[ulPin].ulPin ) == 1) ? HIGH : LOW;
}

#ifdef __cplusplus
}
#endif

