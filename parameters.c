#include "parameters.h"
#include "chash.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static struct chash * parameters;

//XXX TODO: Make this thread safe.
static char returnbuffer[32];

static void Init()
{
	if( !parameters )
	{
		parameters = GenerateHashTable( 0 );
	}
}


float GetParameterF( const char * name, float defa )
{
	struct Param * p = (struct Param*)HashGetEntry( parameters, name );	

	if( p )
	{
		switch( p->t )
		{
		case PFLOAT: return *((float*)p->ptr);
		case PINT:   return *((int*)p->ptr);
		case PSTRING:
		case PBUFFER: if( p->ptr ) return atof( p->ptr );
		default: break;
		}
	}

	return defa;
}

int GetParameterI( const char * name, int defa )
{
	struct Param * p = (struct Param*)HashGetEntry( parameters, name );	

	if( p )
	{
		switch( p->t )
		{
		case PFLOAT: return *((float*)p->ptr);
		case PINT:   return *((int*)p->ptr);
		case PSTRING:
		case PBUFFER: if( p->ptr ) return atoi( p->ptr );
		default: break;
		}
	}

	return defa;
}

const char * GetParameterS( const char * name, const char * defa )
{
	struct Param * p = (struct Param*)HashGetEntry( parameters, name );	

	if( p )
	{
		switch( p->t )
		{
		case PFLOAT: snprintf( returnbuffer, sizeof( returnbuffer ), "%0.4f", *((float*)p->ptr) ); return returnbuffer;
		case PINT:   snprintf( returnbuffer, sizeof( returnbuffer ), "%d", *((int*)p->ptr) );      return returnbuffer;
		case PSTRING:
		case PBUFFER: return p->ptr;
		default: break;
		}
	}

	return defa;
}


static int SetParameter( struct Param * p, const char * str )
{
	switch( p->t )
	{
	case PFLOAT:
		*((float*)p->ptr) = atof( str );
		break;
	case PINT:
		*((int*)p->ptr) = atoi( str );
		break;
	case PBUFFER:
		strncpy( (char*)p->ptr, str, p->size );
		if( p->size > 0 )
			((char*)p->ptr)[p->size-1]= '\0';
		break;
	case PSTRING:
	default:
		return -1;
	}

	struct ParamCallback * cb = p->callback;
	while( cb )
	{
		cb->t( cb->v );
		cb = cb->next;
	}

	return 0;
}

void RegisterValue( const char * name, enum ParamType t, void * ptr, int size )
{
	Init();

	struct Param * p = (struct Param*)HashGetEntry( parameters, name );

	if( p )
	{
		//Entry already exists.
		if( p->orphan )
		{
			if( p->t != PSTRING )
			{
				fprintf( stderr, "Warning: Orphan parameter %s was not a PSTRING.\n", name );
			}
			char * orig = p->ptr;
			p->ptr = ptr;
			p->t = t;
			p->size = size;
			p->orphan = 0;
			int r = SetParameter( p, orig );
			free( orig );
			if( r )
			{
				fprintf( stderr, "Warning: Problem when setting Orphan parameter %s\n", name );
			}
		}
		else
		{
			fprintf( stderr, "Warning: Parameter %s re-registered.  Cannot re-register.\n", name );
		}
	}
	else
	{
		struct Param ** n = (struct Param**)HashTableInsert( parameters, name, 1 );
		*n = malloc( sizeof( struct Param ) );
		(*n)->t = t;
		(*n)->ptr = ptr;
		(*n)->orphan = 0;
		(*n)->size = size;
		(*n)->callback = 0;
	}
}

void SetParametersFromString( const char * string )
{
	char name[PARAM_BUFF];
	char value[PARAM_BUFF];
	char c;

	int namepos = -1; //If -1, not yet found.
	int lastnamenowhite = 0;
	int valpos = -1;
	int lastvaluenowhite = 0;
	char in_value = 0;
	char in_comment = 0;

	while( 1 )
	{
		c = *(string++);
		char is_whitespace = ( c == ' ' || c == '\t' || c == '\r' );
		char is_break = ( c == '\n' || c == ';' || c == 0 );
		char is_comment = ( c == '#' );
		char is_equal = ( c == '=' );

		if( is_comment )
		{
			in_comment = 1;
		}

		if( in_comment )
		{
			if( !is_break )
				continue;
		}

		if( is_break )
		{
			if( namepos < 0 || valpos < 0 ) 
			{
				//Can't do anything with this line.
			}
			else
			{
				name[lastnamenowhite] = 0;
				value[lastvaluenowhite] = 0;
//				printf( "Break: %s %s %d\n", name, value, lastvaluenowhite );

				struct Param * p = (struct Param*)HashGetEntry( parameters, name );
				if( p )
				{
					SetParameter( p, value );
				}
				else
				{
					//p is an orphan.
					struct Param ** n = (struct Param **)HashTableInsert( parameters, name, 0 );
					*n = malloc( sizeof ( struct Param ) );
					(*n)->orphan = 1;
					(*n)->t = PSTRING;
					(*n)->ptr = strdup( value );
					(*n)->size = strlen( value ) + 1;
					(*n)->callback = 0;
				}
			}

			namepos = -1;
			lastnamenowhite = 0;
			valpos = -1;
			lastvaluenowhite = 0;
			in_value = 0;
			in_comment = 0;

			if( c )
				continue;
			else
				break;
		}

		if( is_equal )
		{
			in_value = 1;
			continue;
		}

		if( !in_value )
		{
			if( namepos == -1 )
			{
				if( !is_whitespace )
					namepos = 0;
			}

			if( namepos >= 0 && namepos < PARAM_BUFF )
			{
				name[namepos++] = c;
				if( !is_whitespace )
					lastnamenowhite = namepos;
			}
		}
		else
		{
			if( valpos == -1 )
			{
				if( !is_whitespace )
					valpos = 0;
			}

			if( valpos >= 0 && valpos < PARAM_BUFF )
			{
				value[valpos++] = c;
				if( !is_whitespace )
					lastvaluenowhite = valpos;
			}
		}
	}
}

void AddCallback( const char * name, ParamCallbackT t, void * v )
{
	struct Param * p = (struct Param*)HashGetEntry( parameters, name );	
	if( p )
	{
		struct ParamCallback ** last = &p->callback;
		struct ParamCallback * cb = p->callback;
		while( cb )
		{
			last = &cb->next;
			cb = cb->next;
		}
		cb = *last = malloc( sizeof( struct ParamCallback ) );
		cb->t = t;
		cb->v = v;
		cb->next = 0;
	}
	else
	{
		fprintf( stderr, "Warning: cannot add callback to %s\n.", name );
	}
}

