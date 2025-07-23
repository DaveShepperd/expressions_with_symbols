#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>

#include "lib_formats.h"

typedef struct
{
	char enumName[64];
	char operItem[64];
	int precNorm;
	int precNone;
	char comment[64];
	char fixedComment[128];
} Line_t;

typedef struct
{
	char errName[64];
	char errDesc[128];
} Errors_t;

#define MAX_LINES (64)

#define LibOperStuff_dat "lib_operstuff.dat"

int main(int argc, char *argv[])
{
	char *str, *end, buf[sizeof(Line_t)+5], oBuf[sizeof(Line_t)+5], tokBuf[sizeof(buf)];
	int ii, datLineNo, numLines, numErrs;
	FILE *inF;
	Line_t lines[MAX_LINES], *lp;
	Errors_t errors[MAX_LINES], *ep;
	
	inF = fopen(LibOperStuff_dat,"r");
	if ( !inF )
	{
		perror("Failed to open " LibOperStuff_dat "\n");
		return 1;
	}
	numLines = 0;
	numErrs = 0;
	datLineNo = 0;
	lp = lines;
	ep = errors;
	memset(lp,0,sizeof(lines));
	while ( numLines < MAX_LINES && numErrs < MAX_LINES && fgets(buf, sizeof(buf), inF) )
	{
		++datLineNo;
		end = strchr(buf,'\n');
		if ( !end )
		{
			fprintf(stderr, LibOperStuff_dat ":%d: Malformed entry. No newline found: %s\n", datLineNo, buf);
			end = buf;
		}
		*end = 0;
		memcpy(oBuf,buf,sizeof(oBuf));
		if ( buf[0] == ';' )
			continue;
		if ( buf[0] != 'E' && buf[0] != 'B' )
		{
			printf("%s\n",buf);
			continue;
		}
		memcpy(tokBuf,buf,sizeof(tokBuf));
		tokBuf[sizeof(tokBuf)-1] = 0;
		
		ii = 0;
		str = strtok(tokBuf,",");
		if ( buf[0] == 'B' )
		{
			for (; str; ++ii, str = strtok(NULL, ",") )
			{
				switch (ii)
				{
				case 0:
					continue;
				case 1:
					while ( isspace(*str) )
						++str;
					strncpy(ep->errName,str,sizeof(ep->errName)-1);
					continue;
				case 2:
					while ( isspace(*str) )
						++str;
					strncpy(ep->errDesc,str,sizeof(ep->errDesc)-1);
					continue;
				default:
					fprintf(stderr,"qualtbl.dat:%d: Malformed entry. too many terms: %s\n", datLineNo, buf);
					fclose(inF);
					return 1;
				}
			}
			if ( !ii )
			{
				fprintf(stderr,"qualtbl.dat:%d: Malformed entry. strtok() failed to find term: %s\n", datLineNo, buf);
				fclose(inF);
				return 1;
			}
			++ep;
			++numErrs;
			continue;
		}
		for (; str; ++ii, str = strtok(NULL, ",") )
		{
			switch (ii)
			{
			case 0:
				while ( isspace(*str) )
					++str;
				strncpy(lp->enumName,str,sizeof(lp->enumName)-1);
				continue;
			case 1:
				lp->precNorm = atoi(str);
				continue;
			case 2:
				lp->precNone = atoi(str);
				continue;
			case 3:
				while ( isspace(*str) )
					++str;
				strncpy(lp->comment,str,sizeof(lp->comment)-1);
				continue;
			default:
				fprintf(stderr,"qualtbl.dat:%d: Malformed entry. too many terms: %s\n", datLineNo, buf);
				fclose(inF);
				return 1;
			}
		}
		if ( !ii )
		{
			fprintf(stderr,"qualtbl.dat:%d: Malformed entry. strtok() failed to find term: %s\n", datLineNo, buf);
			fclose(inF);
			return 1;
		}
		if ( lp->comment[0] )
			snprintf(lp->fixedComment, sizeof(lp->fixedComment) - 1, "/* %s: %s", lp->enumName, lp->comment + 3);
		++lp;
		++numLines;
	}
	fclose(inF);
	inF = NULL;
	fprintf(stdout,"/* numLines=%d, __SIZEOF_SIZE_T__=%d, __SIZEOF_INT__=%d, __SIZEOF_LONG__=%d */\n",
			numLines,
			__SIZEOF_SIZE_T__,
			__SIZEOF_INT__,
			__SIZEOF_LONG__);
	fprintf(stdout,	"/* sizeof(char)=" FMT_SZ
					", sizeof(int)=" FMT_SZ
					", sizeof(long)=" FMT_SZ
					", sizeof(void *)=" FMT_SZ
					" */\n/* "
					"sizeof(int8_t)=" FMT_SZ 
					", sizeof(int16_t)=" FMT_SZ 
					", sizeof(int32_t)=" FMT_SZ
					" */\n/* "
					"sizeof(sizeof)=" FMT_SZ
					", sizeof(size_t)=" FMT_SZ
					", sizeof(time_t)=" FMT_SZ 
					" */\n\n"
				,sizeof(char)
				,sizeof(int)
				,sizeof(long)
				,sizeof(void *)
				,sizeof(int8_t)
				,sizeof(int16_t)
				,sizeof(int32_t)
				,sizeof(sizeof(char))
				,sizeof(size_t)
				,sizeof(time_t)
			);
	fputs("#if OPERSTUFF_GET_ENUM\n", stdout);
	fputs("typedef enum\n{\n", stdout);
	lp = lines;
	for (ii=0; ii < numLines; ++ii, ++lp)
	{
		fprintf(stdout,"   %c%s\t%s\n", ii ? ',':' ', lp->enumName, lp->comment );
	}
	fputs("} ExprsTermTypes_t;\n\n"
		  "typedef enum\n{\n"
		  ,stdout);
	ep = errors;
	for (ii=0; ii < numErrs; ++ii, ++ep)
	{
		fprintf(stdout, "   %c%s\t/* %s */\n", ii ? ',':' ', ep->errName, ep->errDesc);
	}
	fputs("} ExprsErrs_t;\n", stdout);
	fputs("#undef OPERSTUFF_GET_ENUM\n"
		  "#endif\n\n"
		  ,stdout);

	fputs("#if OPERSTUFF_GET_OTHERS\n",stdout);
	fputs("static const char *OperDescriptions[] =\n"
		  "{\n"
		  ,stdout);
	lp = lines;
	for (ii=0; ii < numLines; ++ii, ++lp)
	{
		char cmt[64], *src, *dst;
		src = lp->comment+3;
		dst = cmt;
		while ( *src && *src != ')' && dst < cmt+sizeof(cmt)-2)
			*dst++ = *src++;
		*dst++ = ')';
		*dst = 0;
		fprintf(stdout,"   %c\"%s\"\t%s\n", ii ? ',':' ', cmt, lp->fixedComment );
	}
	fputs("};\n\n", stdout);
	fputs("static const ExprsPrecedence_t PrecedenceNormal[] =\n"
		  "{\n"
		  ,stdout);
	lp = lines;
	for (ii=0; ii < numLines; ++ii, ++lp)
	{
		fprintf(stdout, "   %c%2d\t%s\n", ii ? ',':' ', lp->precNorm, lp->fixedComment);
	}
	fputs("};\n\n"
		  "static const ExprsPrecedence_t PrecedenceNone[] =\n"
		  "{\n"
		  ,stdout);
	lp = lines;
	for (ii=0; ii < numLines; ++ii, ++lp)
	{
		fprintf(stdout, "   %c%2d\t%s\n", ii ? ',':' ', lp->precNone, lp->fixedComment);
	}
	fputs("};\n\n", stdout);
	fputs("static const char *ErrorDescriptions[] =\n{\n",stdout);
	ep = errors;
	for (ii=0; ii < numErrs; ++ii, ++ep)
	{
		fprintf(stdout, "   %c%s\t/* %s */\n", ii ? ',':' ', ep->errDesc, ep->errName);
	}
	fputs("};\n", stdout);
	fputs("#undef OPERSTUFF_GET_OTHERS\n"
		  "#endif\n"
		  ,stdout);
	return 0;	
}

