#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>

#include "BOOT_BINGEN.h"

#define DEBUG	0

struct NX_SecondBootInfo* pBootInfo;

static U8* cpu_name			= NULL;
static U8* option_name		= NULL;
static U8* boot_mode		= NULL;
static U8* input_name 		= NULL;
static U8* nsih_name 		= NULL; 
static U8* output_name		= NULL;

static U8* SwapEnb			= NULL;

static U32 device_addr      = CFAILED;
static U32 device_portnum	= CFAILED;

static U32 OutputSize		= NULL; 
static U32 InputSize		= NULL;

static U32 loadaddr			= CFAILED;
static U32 launchaddr		= CFAILED;

int main(int argc, char **argv)
{

	FILE *InFile_fd		= NULL;
	FILE *OutFile_fd	= NULL;

	U32 param_opt 		= NULL;

	U8* Out_Buffer		= NULL;
	U8* pdwBuffer		= NULL;

	U32	MallocSize		= NULL;

	U32 i = NULL, ret	= TRUE;

	U8* view_option		= NULL;

	U8 inFile_Name[50]  = "./bin/";
	U8 outFile_Name[50] = "./output/";
	U8 nsihFile_Name[50]= "./nsih/";

	CRC uCrc;

	/* Pre-Fix  Default (Name & Value) */
	cpu_name	= "S5P4418";
	option_name	= "2ndboot";
	boot_mode	= ".";

	nsih_name	= "NSIH.txt"; 
	input_name	= "pyrope_2ndboot_spi.bin";
	output_name	= "2ndboot_spi.bin";

	OutputSize	= 16*1024;

    if( argc == CTRUE )
    {
        usage();
        return CTRUE;
    }

	while( -1 !=(param_opt = getopt( argc, argv, "hc:t:n:i:o:l:e:a:u:v:")))
	{
      	switch(param_opt)
      	{
	      	case 'h':
	      		usage();
	      		return CTRUE;
	      		break;
	      	case 'c':
	      		cpu_name	= strdup(optarg);
	      		to_upper(cpu_name);
	      		break;				
			case 't':
				option_name = strdup(optarg);	
				to_lower(option_name);
				break;			
	      	case 'n': 
	        	nsih_name 	= strdup(optarg);
	        	break;
	      	case 'i':
	      		input_name 	= strdup(optarg);
	        	break;
	      	case 'o':
	      		output_name = strdup(optarg);
	        	break;      	
			case 'l':
				loadaddr 	= HexAtoInt(optarg);
				break;
	      	case 'e':
	      		launchaddr	= HexAtoInt(optarg);
	        	break;		
	      	case 'u':
	      		device_portnum = HexAtoInt(optarg);
	        	break;	  			
			case 'a':
				device_addr	= HexAtoInt(optarg);					
				break;	
			case 'v':
				view_option	= strdup(optarg);
				break;
			default:
	        	printf("unknown option_num parameter\r\n");
	        	break;
    	}
    }

    if( nsih_name == NULL )
    {
		nsih_name = "NSIH.txt";
		printf("Did not enter the NSIH files.\r\n");
		printf("This has been used as the default NSIH file.\r\n");
    }

	if( input_name == NULL )
	{
		input_name = "pyrope_secondboot.bin";
		printf("Did not enter the Binary files.\r\n");
		printf("This has been used as the default pyrope_secondboot.bin.\r\n");
	}

    InFile_fd	= fopen(input_name , "r");
    OutFile_fd	= fopen(output_name, "w");

    if((!InFile_fd)) 
    {
    	printf("Input File open failed!! check filename!!\n");
    	ret = FALSE;
    	goto ERR_END;
    }

    if((!OutFile_fd))
    {
    	printf("Output File open failed!! check filename!!\n");
    	ret = FALSE;
    	goto ERR_END;
    }

	fseek( InFile_fd, 0, SEEK_END );
	InputSize = ftell ( InFile_fd );
	fseek( InFile_fd, 0, SEEK_SET );

	if( 0 == strcmp( option_name, "2ndboot" ) )
	{
		if( (0 == strcmp( cpu_name, "NXP4330" )) )
		{
		    if( InputSize > (NXP4330_SRAM_SIZE - NSIHSIZE) )
            {
                printf("Enter the binary size exceeded 16KB-512Byte.\n");
                printf("Check Filesize!! (%d) \n", InputSize );
                goto ERR_END;
            }		
			OutputSize = NXP4330_SRAM_SIZE;
		}
		else if( (0 == strcmp( cpu_name, "NXP5430" )) || (0 == strcmp( cpu_name, "S5P6818" )) )
		{
			OutputSize = InputSize + NSIHSIZE;
			if( OutputSize >= (NXP5430_SRAM_SIZE - ROMBOOT_STACK_SIZE) )
			{
				printf("The image is Generated exceeds 64KB. The Creation failed! \r\n");
				printf("Calcurate image Size : %d \r\n", OutputSize );
				printf("Return Error End!!\r\n");
				goto ERR_END;
			}		
		}		
		else if(0 == strcmp( cpu_name, "S5P4418" ))
		{
			OutputSize = InputSize + NSIHSIZE;	
			if( OutputSize >= (S5P4418_SRAM_SIZE - ROMBOOT_STACK_SIZE) )
			{
				printf("The image is Generated exceeds 28KB. The Creation failed! \r\n");
				printf("Calcurate image Size : %d \r\n", OutputSize );
				printf("Return Error End!!\r\n");
				goto ERR_END;
			}		
			else if( OutputSize < NXP4330_SRAM_SIZE ) // S5P 4418 BinGen Size < 16*1024
			{
                OutputSize = NXP4330_SRAM_SIZE;
			}
		}
		else
			printf("CPU name is unknown. \r\n");     

	}
	else if( 0 == strcmp( option_name, "3rdboot" ) )
		OutputSize = InputSize + NSIHSIZE;

	if( OutputSize == 0 )	
	{
		printf("Did not enter the Filesize files.\r\n");
		goto ERR_END;
	}
    else
    {
        if( (OutputSize <= (16*1024)) )
            OutputSize = 16*1024;
    }
	
#if 0
	if( (OutputSize % 512) != 0 )
		MallocSize = ((OutputSize / BLOCKSIZE) + 1) * BLOCKSIZE;
	else
#endif
		MallocSize = OutputSize;

    Out_Buffer 	= (U8*)malloc( MallocSize );
	memset(Out_Buffer , 0xFF, MallocSize);	
	

 	if( NSIHSIZE != ProcessNSIH( nsih_name, Out_Buffer ) )
	{
		printf("ERROR : Failed to process NSIH(%s).\n", argv[3] );
		ret = FALSE;
		goto ERR_END;
	}
	if( 0 > fread(Out_Buffer + NSIHSIZE, 1, InputSize, InFile_fd) )
	{
        printf("File Read Failed. \r\n");
        return CFAILED;
	}

#if SECURE_BOOT
	// Secure Boot <-- Decript Issue ( 16Byte Convert )
	if( ((InputSize % 16) != 0) )	
		InputSize = ((InputSize / 16) * 16);
#endif

	//if( (0 == strcmp( option_name, "3rdboot" )) || (0 == strcmp( option_name, "2ndboot" )) )
	{	
		pBootInfo = (struct NX_SecondBootInfo*)Out_Buffer;

        if( device_portnum != CFAILED )
        {
    		pBootInfo->DBI.SDMMCBI.PortNumber = (U8)device_portnum;     
        }
		if( device_addr != CFAILED )
		{
            pBootInfo->DEVICEADDR = device_addr;                        
		}	
		
		pBootInfo->LOADSIZE			= InputSize;	

        if( loadaddr != CFAILED )
    		pBootInfo->LOADADDR			= loadaddr; 
        if( launchaddr != CFAILED )
	    	pBootInfo->LAUNCHADDR		= launchaddr;

		pBootInfo->SIGNATURE		= HEADER_ID;		
        pBootInfo->DBI.SDMMCBI.CRC32 = __calc_crc((void*)(Out_Buffer + NSIHSIZE), (InputSize) );
		if( (InputSize + NSIHSIZE) <= (NXP4330_SRAM_SIZE-16) )
		{
			uCrc.iCrc = __calc_crc((void*)(Out_Buffer), (NXP4330_SRAM_SIZE-16) );		
			for(i = 0; i < CRCSIZE; i++)
			{	
				Out_Buffer[NXP4330_SRAM_SIZE-16+i] = uCrc.chCrc[i];
            #if 0
				printf("(BASEADDR + 0x%X) - CRC[%d]: %X(%X) \r\n", 
				(OutputSize-16+i), i, Out_Buffer[OutputSize-16+i], uCrc.chCrc[i]);
		    #endif
			}
		}		
	}	

    U8* pBoodMode = (U8*)(&pBootInfo->DBI.SPIBI._Reserved1);
    if( pBoodMode[3] == BOOT_FROM_USB )
        boot_mode = "USB";
    else if( pBoodMode[3] == BOOT_FROM_SPI )
        boot_mode = "SPI";
    else if( pBoodMode[3] == BOOT_FROM_NAND )
        boot_mode = "NAND";
    else if( pBoodMode[3] == BOOT_FROM_SDMMC )
        boot_mode = "SDMMC";
    else if( pBoodMode[3] == BOOT_FROM_SDFS )
        boot_mode = "SDFS";
    else if( pBoodMode[3] == BOOT_FROM_UART )
        boot_mode = "UART";

	fwrite(Out_Buffer, 1, OutputSize, OutFile_fd);		
	print_bingen_info();			

#if 0
	if( (0 == strcmp( view_option, "viewer" )) ){
		//print_hexdump( (U32*)Out_Buffer + (NSIHSIZE/4), InputSize );
		NX_SHELLU_HexDump( (U32*)Out_Buffer, OutputSize, 8 ); 	}
#endif
		
	fclose(InFile_fd);
	fclose(OutFile_fd);

	free(Out_Buffer);

NOMAL_END:
	return 0;

ERR_END:
	fclose(InFile_fd);
	fclose(OutFile_fd);

	free(Out_Buffer);	

	return -1;
}

void print_hexdump( U32* pdwBuffer, U32 Size )
{
	register int i = 0;
	
	for(i = 0; i < Size/4; i++)
	{
		printf("%8X  ", pdwBuffer[i] );
		if( ((i+1) % 8) == 0 )
			printf("\r\n");	
	}
	printf("\r\n");
}

void to_upper( char* string )
{
    char*   str = (char*)string;
    int     ndx = 0;

    for(ndx = 0; ndx < strlen(str); ndx++)
    {
        str[ndx] = (char)toupper(str[ndx]);
    }
}

void to_lower( char* string )
{
    char*   str = (char*)string;
    int     ndx = 0;

    for(ndx = 0; ndx < strlen(str); ndx++)
    {
        str[ndx] = (char)tolower(str[ndx]);
    }
}


#if 0
//------------------------------------------------------------------------------
// NX_SHELLU_HexDump
//------------------------------------------------------------------------------
CBOOL	NX_SHELLU_HexDump	( U32 SrcAddr, U32 PrintDataSize, U32 PrintDataWidth )
{
    U32 i, j;
    U32 dwMemAddr;
    U32 Buffer[16/4];
    U8 	*pBuffer8;
    U16 *pBuffer16;
    U32 *pBuffer32;

    U32 Cnt = 0;

    pBuffer8 	= (U8  *)Buffer;
    pBuffer16 	= (U16 *)Buffer;
    pBuffer32 	= (U32 *)Buffer;

    // checke data width size
         if( PrintDataWidth == 16 ) dwMemAddr = (U32)(SrcAddr & ~0x1);
    else if( PrintDataWidth == 32 ) dwMemAddr = (U32)(SrcAddr & ~0x3);
    else 							dwMemAddr = (U32)(SrcAddr & ~0x0);

    // Print data
	for( i=0 ; i < (PrintDataSize/16) ; i++ )
	{
		//printf("0x%08X", dwMemAddr );
		//printf( " : " );

        // 16bit expressiont case
		if( PrintDataWidth == 16 )
		{
			pBuffer16[0] = *(((U16 *)dwMemAddr)+0);
			pBuffer16[1] = *(((U16 *)dwMemAddr)+1);
			pBuffer16[2] = *(((U16 *)dwMemAddr)+2);
			pBuffer16[3] = *(((U16 *)dwMemAddr)+3);
			pBuffer16[4] = *(((U16 *)dwMemAddr)+4);
			pBuffer16[5] = *(((U16 *)dwMemAddr)+5);
			pBuffer16[6] = *(((U16 *)dwMemAddr)+6);
			pBuffer16[7] = *(((U16 *)dwMemAddr)+7);

        	printf("%04X  "   , pBuffer16[0] );
        	printf("%04X  "   , pBuffer16[1] );
        	printf("%04X  "   , pBuffer16[2] );
        	printf("%04X  :  ", pBuffer16[3] );
        	printf("%04X  "   , pBuffer16[4] );
        	printf("%04X  "   , pBuffer16[5] );
        	printf("%04X  "   , pBuffer16[6] );
        	printf("%04X "    , pBuffer16[7] );
        }
	    // 32bit expressiont case
		else if( PrintDataWidth == 32 )
		{
			pBuffer32[0] = *(((U32 *)dwMemAddr)+0);
			pBuffer32[1] = *(((U32 *)dwMemAddr)+1);
			pBuffer32[2] = *(((U32 *)dwMemAddr)+2);
			pBuffer32[3] = *(((U32 *)dwMemAddr)+3);

        	printf("%08X    "     , pBuffer32[0] );
        	printf("%08X    :    ", pBuffer32[1] );
        	printf("%08X    "     , pBuffer32[2] );
        	printf("%08X "        , pBuffer32[3] );
        }
        // 8bit expressiont case
		else //if( PrintDataWidth == 8 )
		{
			pBuffer8[ 0] = *(((U8 *)dwMemAddr)+ 0);
			pBuffer8[ 1] = *(((U8 *)dwMemAddr)+ 1);
			pBuffer8[ 2] = *(((U8 *)dwMemAddr)+ 2);
			pBuffer8[ 3] = *(((U8 *)dwMemAddr)+ 3);
			pBuffer8[ 4] = *(((U8 *)dwMemAddr)+ 4);
			pBuffer8[ 5] = *(((U8 *)dwMemAddr)+ 5);
			pBuffer8[ 6] = *(((U8 *)dwMemAddr)+ 6);
			pBuffer8[ 7] = *(((U8 *)dwMemAddr)+ 7);
			pBuffer8[ 8] = *(((U8 *)dwMemAddr)+ 8);
			pBuffer8[ 9] = *(((U8 *)dwMemAddr)+ 9);
			pBuffer8[10] = *(((U8 *)dwMemAddr)+10);
			pBuffer8[11] = *(((U8 *)dwMemAddr)+11);
			pBuffer8[12] = *(((U8 *)dwMemAddr)+12);
			pBuffer8[13] = *(((U8 *)dwMemAddr)+13);
			pBuffer8[14] = *(((U8 *)dwMemAddr)+14);
			pBuffer8[15] = *(((U8 *)dwMemAddr)+15);

        	printf("%02X "  , pBuffer8[ 0] );
        	printf("%02X "  , pBuffer8[ 1] );
        	printf("%02X "  , pBuffer8[ 2] );
        	printf("%02X "  , pBuffer8[ 3] );
        	printf("%02X "  , pBuffer8[ 4] );
        	printf("%02X "  , pBuffer8[ 5] );
        	printf("%02X "  , pBuffer8[ 6] );
        	printf("%02X ", pBuffer8[ 7] );
        	printf("%02X "  , pBuffer8[ 8] );
        	printf("%02X "  , pBuffer8[ 9] );
        	printf("%02X "  , pBuffer8[10] );
        	printf("%02X "  , pBuffer8[11] );
        	printf("%02X "  , pBuffer8[12] );
        	printf("%02X "  , pBuffer8[13] );
        	printf("%02X "  , pBuffer8[14] );
        	printf("%02X "  , pBuffer8[15] );

        	Cnt+=16;
		}

    	// Print 16 byte character information
    	/*
    	for( j=0 ; j<16 ; j++ )
    	{
	    	if( pBuffer8[j] >= 0x21 &&
	    	    pBuffer8[j] <= 0x7e &&
	    	    pBuffer8[j] != '%' )
	    	    printf( "%c", pBuffer8[j] );
	      	else
	      	    printf( "." );
        }
		*/
	    printf("\r\n");

		dwMemAddr += 16;
	}
	printf("Cnt : %d \r\n", Cnt );

	return CTRUE;
}
#endif

static unsigned int globalk = 0;
unsigned int get_fcs(unsigned int fcs, unsigned char data)
{
	register int i;
	fcs ^= (unsigned int)data;
	for(i = 0; i < 8; i++)
	{
	   if(fcs & 0x01) 
	   		fcs ^= POLY; 
	   fcs >>= 1;
	}
	return fcs;
}


static inline unsigned int __calc_crc (void *addr, int len)
{
	U8 *c = (U8*)addr;
	U32 crc = 0;
	int i;
	for (i = 0; len > i; i++)
	{
		crc = get_fcs(crc, c[i]);
	}
	return crc;
}


// Nexell System Information Header
//------------------------------------------------------------------------------
int ProcessNSIH( const char *pfilename, U8 *pOutData )
{
	FILE	*fp;
	char ch;
	int writesize, skipline, line, bytesize, i;
	U32 writeval;

	fp = fopen( pfilename, "rb" );
	if( !fp )
	{
		printf( "ProcessNSIH : ERROR - Failed to open %s file.\n", pfilename );
		return 0;
	}

	bytesize = 0;
	writeval = 0;
	writesize = 0;
	skipline = 0;
	line = 0;

	while( 0 == feof( fp ) )
	{
		ch = fgetc( fp );

		if( skipline == 0 )
		{
			if( ch >= '0' && ch <= '9' )
			{
				writeval = writeval * 16 + ch - '0';
				writesize += 4;
			}
			else if( ch >= 'a' && ch <= 'f' )
			{
				writeval = writeval * 16 + ch - 'a' + 10;
				writesize += 4;
			}
			else if( ch >= 'A' && ch <= 'F' )
			{
				writeval = writeval * 16 + ch - 'A' + 10;
				writesize += 4;
			}
			else
			{
				if( writesize == 8 || writesize == 16 || writesize == 32 )
				{
					for( i=0 ; i<writesize/8 ; i++ )
					{
						pOutData[bytesize++] = (U8)(writeval & 0xFF);
						writeval >>= 8;
					}
				}
				else
				{
					if( writesize != 0 )
						printf("ProcessNSIH : Error at %d line.\n", line+1 );
				}
				writesize = 0;
				skipline = 1;
			}
		}

		if( ch == '\n' )
		{
			line++;
			skipline = 0;
			writeval = 0;
		}
	}

	NX_DEBUG_MSG( "ProcessNSIH : %d line processed.\n", line+1 );
	NX_DEBUG_MSG( "ProcessNSIH : %d bytes generated.\n", bytesize );

	fclose( fp );

	return bytesize;
}

U32 HexAtoInt( const char *string )
{
	char 	ch;
	U32 	result = 0;

	while( (ch = *string++) != 0 )
	{
		if( ch >= '0' && ch <= '9' )
		{
			result = result * 16 + (ch - '0');
		}
		else if( ch >= 'a' && ch <= 'f' )
		{
			result = result * 16 + (ch - 'a') + 10;
		}
		else if( ch >= 'A' && ch <= 'F' )
		{
			result = result * 16 + (ch - 'A') + 10;
		}
	}

	return result;
}


static void usage(void)
{
	printf("--------------------------------------------------------------------------\n");
	printf(" Release  Version         : Ver.%04d                                      \n", BOOT_BINGEN_VER );
	printf(" Author & Version Manager : Deoks (S/W 1Team)                             \n");
	printf("--------------------------------------------------------------------------\n");
	
	printf(" Usage : This will tell you How to Use Help.					          \n");
	printf("--------------------------------------------------------------------------\n" );
	printf("   -h [HELP]                     : show usage                             \n");
	printf("   -c [S5P6818/S5P4418]          : chip name         (mandatory)          \n");	
	printf("   -t [2nboot/3rdboot]           : What is the Boot? (mandatory)          \n");
	printf("   	->[2ndboot]                                                           \n");
	printf("   	->[3rdboot]              	   	    	                              \n");
	printf("   -n [file name]                : [NSIH] file name	 (mandatory)     	  \n");
	printf("   -i [file name]                : [INPUT]file name  (mandatory)		  \n");
	printf("   -o [file name]                : [OUTPUT]file name (mandatory)    	  \n");
	printf("   -l [load address]             : Binary Load  Address              	  \n");
	printf("   	-> Default Load	  Address : Default NSIH.txt   				          \n");			
	printf("   -e [launch address]           : Binary Launch Addres             	  \n");
	printf("   	-> Default Launch Address : Default NSIH.txt   				          \n");
	printf("--------------------------------------------------------------------------\n");
    printf("   This Option is Not Required. (if you do not use NSIH.txt Default.)     \n");
    printf("--------------------------------------------------------------------------\n");
	printf("   -a [device address]           : What is the Device Address?   		  \n");
	printf("   -u [device port]              : device channel                         \n");
    printf("--------------------------------------------------------------------------\n");
    printf(" Remark & Reference Message                                               \n");
    printf(" The current version has not been applied to the NAND BINGEN version..    \n");
    printf("--------------------------------------------------------------------------\n");
	printf("\n");
	printf(" Usage: How to use the program? 			                              \n"); 
	printf(" Ubuntu  > How to use?                                                    \n");
	printf("  #>./BOOT_BINGEN -h 0 or ./BOOT_BINGEN \n");
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_spi_800.txt -i pyrope_2ndboot_spi.bin -o 2ndboot_spi.bin \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_sd_800.txt  -i pyrope_2ndboot_sdmmc.bin -o 2ndboot_sdmmc.bin \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_spi_800.txt -i u-boot.bin -o 3rdboot_spi.bin -l 40100000 -e 40100000 \n" );
	printf("  #>./BOOT_BINGEN -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_sd_800.txt  -i u-boot.bin -o 3rdboot_sdmmc.bin -l 40100000 -e 40100000 \n" );
    printf("\n");
	printf(" Windows > How to use?                                                    \n");
    printf("  #>BOOT_BINGEN.exe -h 0 or BOOT_BINGEN.exe \n");
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_spi_800.txt -i pyrope_2ndboot_spi.bin -o 2ndboot_spi.bin \n" );
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 2ndboot -n NXP4330_NSIH_V05_sd_800.txt  -i pyrope_2ndboot_sdmmc.bin -o 2ndboot_sdmmc.bin \n" );
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_spi_800.txt -i u-boot.bin -o 3rdboot_spi.bin -l 40100000 -e 40100000 \n" );
	printf("  #>BOOT_BINGEN.exe -c NXP4330 -t 3rdboot -n NXP4330_NSIH_V05_sd_800.txt  -i u-boot.bin -o 3rdboot_sdmmc.bin -l 40100000 -e 40100000 \n" );	

	printf("\n");
}

static void print_bingen_info( void )
{
	printf( "----------------------------------------------------\n" );
	printf( " %s %s Binary file Information.\n", cpu_name, option_name );
	printf( " %s Binary Boot Mode 	 		\n", boot_mode );
	printf( " NSIH   Text   File : %s		\n", nsih_name   ? (char*)nsih_name   : "NULL" );
	printf( " Input  Binary File : %s		\n", input_name  ? (char*)input_name  : "NULL" );
	printf( " Output Binary File : %s		\n", output_name ? (char*)output_name : "NULL" );
	printf( " Input  Binary Size : %d Byte (%dKB)	\n", InputSize   ? InputSize   : 0, (InputSize+1024-1)/1024 );
	printf( " Output Binary Size : %d Byte (%dKB)\n", OutputSize  ? OutputSize  : 0, (OutputSize+1024-1)/1024 );
	printf( "----------------------------------------------------\n" );
	printf( " NSIH(Header) Information.  \n" );
	printf( " %s Binary Boot Mode 	\n", boot_mode );
	
	if( (0 == strcmp( boot_mode, "SPI" )) 
	|| (pBootInfo->DBI.SPIBI._Reserved1 == 0x01000000 ) )
		printf( "  -> Addr  Step : %8Xh \r\n", pBootInfo->DBI.SPIBI.AddrStep );
	else
		printf( "  -> DevicePort : %8Xh \r\n", pBootInfo->DBI.SDMMCBI.PortNumber );
	
	printf( "  -> DeviceAddr : %8Xh \r\n", pBootInfo->DEVICEADDR );
	printf( "  -> LoadSize	: %8Xh \r\n", pBootInfo->LOADSIZE   );
	printf( "  -> LoadAddr	: %8Xh \r\n", pBootInfo->LOADADDR   );
	printf( "  -> LauchAddr	: %8Xh \r\n", pBootInfo->LAUNCHADDR );
	printf( "  -> SigNature	: %8Xh \r\n", pBootInfo->SIGNATURE );
	printf( "  -> CRC32	: %8Xh \r\n", pBootInfo->DBI.SDMMCBI.CRC32 );
}

