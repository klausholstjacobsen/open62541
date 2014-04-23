/*
 * xml2ns0.c
 *
 *  Created on: 21.04.2014
 *      Author: mrt
 */

#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // strlen
#include <ctype.h> // isspace
#include <unistd.h> // read

#include "opcua.h"
#include "ua_namespace.h"


typedef char const * const XML_Attr_t;
typedef char const * cstring_t;
struct XML_Stack;

typedef UA_Int32 (*XML_decoder)(struct XML_Stack* s, XML_Attr_t* attr, void* dst, _Bool isStart);

typedef struct XML_child {
	cstring_t name;
	UA_Int32 type;
	XML_decoder elementHandler;
	void* obj;
} XML_child_t;

typedef struct XML_Parent {
	cstring_t name;
	int textAttribIdx; // -1 - not set
	cstring_t textAttrib;
	int activeChild; // -1 - no active child
	int len; // -1 - empty set
	XML_child_t children[20];
} XML_Parent_t;

typedef struct XML_Stack {
	int depth;
	XML_Parent_t parent[10];
} XML_Stack_t;

void XML_Stack_init(XML_Stack_t* p, cstring_t name) {
	unsigned int i,j;
	p->depth = 0;
	for (i=0;i<10;i++) {
		p->parent[i].name = UA_NULL;
		p->parent[i].len = 0;
		p->parent[i].activeChild = -1;
		p->parent[i].textAttrib = UA_NULL;
		p->parent[i].textAttribIdx = -1;
		for (j=0;j<20;j++) {
			p->parent[i].children[j].name = UA_NULL;
			p->parent[i].children[j].elementHandler = UA_NULL;
			p->parent[i].children[j].type = UA_INVALIDTYPE;
			p->parent[i].children[j].obj = UA_NULL;
		}
	}
	p->parent[0].name = name;
}

void XML_Stack_print(XML_Stack_t* s) {
	UA_Int32 i;
	for (i=0;i<=s->depth;i++) {
		printf("%s.",s->parent[i].name);
	}
}

// FIXME: we might want to calculate textAttribIdx
void XML_Stack_handleTextAs(XML_Stack_t* p,cstring_t textAttrib, unsigned int textAttribIdx) {
	p->parent[p->depth].textAttrib = textAttrib;
	p->parent[p->depth].textAttribIdx = textAttribIdx;
}

void XML_Stack_addChildHandler(XML_Stack_t* p,cstring_t name,XML_decoder handler, UA_Int32 type, void* dst) {
	unsigned int len = p->parent[p->depth].len;
	p->parent[p->depth].children[len].name = name;
	p->parent[p->depth].children[len].elementHandler = handler;
	p->parent[p->depth].children[len].type = type;
	p->parent[p->depth].children[len].obj = dst;
	p->parent[p->depth].len++;
}


typedef struct T_UA_NodeSet {
	namespace* ns;
} UA_NodeSet;

UA_Int32 UA_NodeSet_new(UA_NodeSet** p) {
	UA_alloc((void**)&p,sizeof(UA_NodeSet));
	create_ns(&((*p)->ns),100);
	return UA_SUCCESS;
}

UA_Int32 UA_NodeId_copycstring(cstring_t src,UA_NodeId* dst) {
	dst->encodingByte = UA_NODEIDTYPE_FOURBYTE;
	dst->namespace = 0;
	// FIXME: assumes i=nnnn, does not care for aliases as of now
	dst->identifier.numeric = atoi(&src[2]);
	return UA_SUCCESS;
}

UA_Int32 UA_Array_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, void* dst, _Bool isStart) {
	// FIXME: Implement
	return UA_SUCCESS;
}

UA_Int32 UA_Int32_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, UA_Int32* dst, _Bool isStart) {
	if (isStart) {
		if (dst == UA_NULL) {
			UA_Int32_new(&dst);
			s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = (void*) dst;
		}
		*dst = atoi(attr[1]);
	} else {
		// TODO: It is a design flaw that we need to do this here, isn't it?
		s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

UA_Int32 UA_String_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, UA_String* dst, _Bool isStart) {
	UA_UInt32 i;
	if (isStart) {
		if (dst == UA_NULL) {
			UA_String_new(&dst);
			s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = (void*) dst;
		}
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s,"Data",(XML_decoder)UA_Array_decodeXML, UA_BYTE, &(dst->data));
		XML_Stack_addChildHandler(s,"Length",(XML_decoder)UA_Int32_decodeXML, UA_INT32, &(dst->length));
		XML_Stack_handleTextAs(s,"Data",0);

		// set attributes
		for (i = 0; attr[i]; i += 2) {
			if (0==strncmp("Data",attr[i],strlen("Data"))) {
				UA_String_copycstring(attr[i+1],dst);
			} else {
				perror("Unknown attribute");
			}
		}
	} else {
		// TODO: It is a design flaw that we need to do this here, isn't it?
		s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

UA_Int32 UA_LocalizedText_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, UA_LocalizedText* dst, _Bool isStart) {
	UA_UInt32 i;
	if (isStart) {
		if (dst == UA_NULL) {
			UA_LocalizedText_new(&dst);
			s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = (void*) dst;
		}
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s,"Text",(XML_decoder)UA_String_decodeXML, UA_STRING, &(dst->text));
		XML_Stack_addChildHandler(s,"Locale",(XML_decoder)UA_String_decodeXML, UA_STRING, &(dst->locale));
		XML_Stack_handleTextAs(s,"Data",0);

		// set attributes
		for (i = 0; attr[i]; i += 2) {
			if (0==strncmp("Text",attr[i],strlen("Text"))) {
				UA_String_copycstring(attr[i+1],&(dst->text));
				dst->encodingMask |= UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT;
			} else if (0==strncmp("Locale",attr[i],strlen("Locale"))) {
				UA_String_copycstring(attr[i+1],&(dst->locale));
				dst->encodingMask |= UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_LOCALE;
			} else {
				perror("Unknown attribute");
			}
		}
	} else {
		switch (s->parent[s->depth].activeChild) {
		case 0:
			dst->encodingMask |= UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT;
			break;
		case 1:
			dst->encodingMask |= UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_LOCALE;
			break;
		default:
			break;
		}
		// TODO: I think it is a design flaw that we need to do this here, isn't it?
		s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

UA_Int32 UA_DataTypeNode_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, UA_DataTypeNode* dst, _Bool isStart) {
	UA_UInt32 i;

	if (isStart) {
		// create a new object if called with UA_NULL
		if (dst == UA_NULL) {
			UA_DataTypeNode_new(&dst);
			s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = (void*) dst;
		}

		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s,"DisplayName",(XML_decoder)UA_LocalizedText_decodeXML, UA_LOCALIZEDTEXT, &(dst->displayName));
		XML_Stack_addChildHandler(s,"Description",(XML_decoder)UA_LocalizedText_decodeXML, UA_LOCALIZEDTEXT, &(dst->description));

		// set missing default attributes
		dst->nodeClass = UA_NODECLASS_DATATYPE;

		// set attributes
		for (i = 0; attr[i]; i += 2) {
			if (0==strncmp("NodeId",attr[i],strlen("NodeId"))) {
				UA_NodeId_copycstring(attr[i+1],&(dst->nodeId));
			} else if (0==strncmp("BrowseName",attr[i],strlen("BrowseName"))) {
				UA_String_copycstring(attr[i+1],&(dst->browseName.name));
				dst->browseName.namespaceIndex = 0;
			} else if (0==strncmp("DisplayName",attr[i],strlen("DisplayName"))) {
				UA_String_copycstring(attr[i+1],&(dst->displayName.text));
				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT;
			} else if (0==strncmp("Description",attr[i],strlen("Description"))) {
				UA_String_copycstring(attr[i+1],&(dst->description.text));
				dst->description.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT;
			} else {
				DBG_ERR(XML_Stack_print(s));
				DBG_ERR(printf("%s - unknown attribute\n",attr[i]));
			}
		}
	} else {
		// TODO: It is a design flaw that we need to do this here, isn't it?
		s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

void print_node(UA_Node const * node) {
	if (node != UA_NULL) {
		UA_NodeId_printf("---------------------------\nnode.NodeId=", &(node->nodeId));
		printf("node.browseName='%.*s'\n", node->browseName.name.length, node->browseName.name.data);
	}
}

typedef struct UA_NodeSetAlias_T {
	UA_String alias;
	UA_String value;
} UA_NodeSetAlias;

UA_Int32 UA_NodeSetAlias_new(UA_NodeSetAlias** p) {
	UA_alloc((void**)p,sizeof(UA_NodeSetAlias));
	UA_String_init(&((*p)->alias));
	UA_String_init(&((*p)->value));
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSetAlias_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, UA_NodeSetAlias* dst, _Bool isStart) {
	if (isStart) {
		// create if necessary
		if (dst == UA_NULL) {
			UA_NodeSetAlias_new(&dst);
			s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = (void*) dst;
		}
		// set handlers
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s,"Alias",(XML_decoder) UA_String_decodeXML, UA_STRING, &(dst->alias));
		XML_Stack_addChildHandler(s,"Value",(XML_decoder) UA_String_decodeXML, UA_STRING, &(dst->value));
		XML_Stack_handleTextAs(s,"Data",1);

		// set attributes
		UA_Int32 i;
		for (i = 0; attr[i]; i += 2) {
			if (0==strncmp("Alias",attr[i],strlen("Alias"))) {
				UA_String_copycstring(attr[i+1],&(dst->alias));
			} else if (0==strncmp("Value",attr[i],strlen("Value"))) {
				UA_String_copycstring(attr[i+1],&(dst->value));
			} else {
				DBG_ERR(XML_Stack_print(s));
				DBG_ERR(printf("%s - unknown attribute\n",attr[i]));
			}
		}
	} else {
		// sub element is ready
		// TODO: It is a design flaw that we need to do this here, isn't it?
		s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

typedef struct UA_NodeSetAliases_T {
	UA_Int32 size;
	UA_NodeSetAlias** aliases;
} UA_NodeSetAliases;

UA_Int32 UA_NodeSetAliases_new(UA_NodeSetAliases** p) {
	UA_alloc((void**)p,sizeof(UA_NodeSetAliases));
	(*p)->size = 0;
	(*p)->aliases = UA_NULL;
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSetAliases_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, UA_NodeSetAliases* dst, _Bool isStart) {
	if (isStart) {
		if (dst == UA_NULL) {
			UA_NodeSetAliases_new(&dst);
			s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = (void*) dst;
		}
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s,"Aliases",(XML_decoder) UA_NodeSetAlias_decodeXML, UA_INVALIDTYPE, UA_NULL);
	} else {
		// sub element is ready, add to array
		if (dst->size < 0 || dst->size ==  0) {
			dst->size = 1;
			UA_alloc((void**)&(dst->aliases), dst->size*sizeof(UA_NodeSetAlias*));
		} else {
			dst->size++;
			dst->aliases = realloc(dst->aliases,dst->size*sizeof(UA_NodeSetAlias*));
		}
		// index starts with 0, therefore size-1
		dst->aliases[dst->size-1] = (UA_NodeSetAlias*) attr;
		// TODO: It is a design flaw that we need to do this here, isn't it?
		s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}


UA_Int32 UA_NodeSet_decodeXML(XML_Stack_t* s, XML_Attr_t* attr, UA_NodeSet* dst, _Bool isStart) {
	if (isStart) {
		if (dst == UA_NULL) {
			UA_NodeSet_new(&dst);
			s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = (void*) dst;
		}
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s,"UADataType",(XML_decoder)UA_DataTypeNode_decodeXML, UA_DATATYPENODE, UA_NULL);
		XML_Stack_addChildHandler(s,"Aliases",(XML_decoder) UA_NodeSetAliases_decodeXML, UA_INVALIDTYPE, UA_NULL);
	} else {
		if (attr != UA_NULL) {
			UA_Node* node = (UA_Node*) attr;
			insert_node(dst->ns, node);
			DBG_VERBOSE(printf("Inserting "));
			DBG_VERBOSE(print_node(node));
		} else {
			DBG_ERR(printf("nodeset endElement called with null-ptr\n"));
		}
		// TODO: It is a design flaw that we need to do this here, isn't it?
		s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

/** lookup if element is a known child of parent, if yes go for it otherwise ignore */
void startElement(void * data, const char *el, const char **attr) {
  XML_Stack_t* s = (XML_Stack_t*) data;
  int i;

  // scan expected children
  XML_Parent_t* cp = &s->parent[s->depth];
  for (i = 0; i < cp->len; i++) {
	  if (0 == strncmp(cp->children[i].name,el,strlen(cp->children[i].name))) {
		  DBG_VERBOSE(XML_Stack_print(s));
		  DBG_VERBOSE(printf("%s - processing\n",el));

		  cp->activeChild = i;

		  s->depth++;
		  s->parent[s->depth].name = el;
		  s->parent[s->depth].len = 0;
		  s->parent[s->depth].textAttribIdx = -1;
		  s->parent[s->depth].activeChild = -1;

		  // finally call the elementHandler and return
		  cp->children[i].elementHandler(data,attr,cp->children[i].obj, TRUE);
		  return;
	  }
  }
  // if we come here we rejected the processing of el
  DBG_VERBOSE(XML_Stack_print(s));
  DBG_VERBOSE(printf("%s - rejected\n",el));
  s->depth++;
  s->parent[s->depth].name = el;
  // this should be sufficient to reject the children as well
  s->parent[s->depth].len = 0;
}

UA_Int32 XML_isSpace(cstring_t s, int len) {
	int i;
	for (i=0; i<len; i++) {
		if (! isspace(s[i])) {
		  return UA_FALSE;
		}
	}
	return UA_TRUE;
}

/* simulates startElement, endElement behaviour */
void handleText(void * data, const char *txt, int len) {
  XML_Stack_t* s = (XML_Stack_t*) data;

  if (len > 0 && ! XML_isSpace(txt,len)) {
	  XML_Parent_t* cp = &(s->parent[s->depth]);
	  if (cp->textAttribIdx >= 0) {
		  cp->activeChild = cp->textAttribIdx;
		  char* buf; // need to copy txt to add 0 as string terminator
		  UA_alloc((void**)&buf,len+1);
		  strncpy(buf,txt,len);
		  buf[len] = 0;
		  XML_Attr_t attr[3] = { cp->textAttrib, buf, UA_NULL };
		  cp->children[cp->activeChild].elementHandler(s,attr,cp->children[cp->activeChild].obj, TRUE);
		  cp->children[cp->activeChild].elementHandler(s,UA_NULL,cp->children[cp->activeChild].obj, FALSE);
		  UA_free(buf);
	  } else {
		  DBG_ERR(XML_Stack_print(s));
		  DBG_ERR(printf("textData - ignore text data '%.*s'\n",len,txt));
	  }
  }
}

/** if we are an activeChild of a parent we call the child-handler */
void endElement(void *data, const char *el) {
	XML_Stack_t* s = (XML_Stack_t*) data;

	// the parent knows the elementHandler, therefore depth-1 !
	if (s->depth>1) {
		// inform parents elementHandler that everything is done
		XML_Parent_t* cp = &(s->parent[s->depth-1]);
		XML_Parent_t* cpp = &(s->parent[s->depth-2]);
		if (cpp->activeChild >= 0 && cp->activeChild >= 0) {
			DBG_VERBOSE(XML_Stack_print(s));
			DBG_VERBOSE(printf(" - inform parent %s\n", cpp->children[cpp->activeChild].name));
			cpp->children[cpp->activeChild].elementHandler(s,(XML_Attr_t*)cp->children[cp->activeChild].obj,cpp->children[cpp->activeChild].obj, FALSE);
		}
		// reset
		cp->activeChild = -1;
	}
	s->depth--;
}


int main()
{
  char buf[1024];
  int len;   /* len is the number of bytes in the current bufferful of data */
  XML_Stack_t s;
  XML_Stack_init(&s, "ROOT");
  UA_NodeSet n;
  create_ns(&(n.ns),100);
  XML_Stack_addChildHandler(&s,"UANodeSet", (XML_decoder) UA_NodeSet_decodeXML, UA_INVALIDTYPE, &n);

  XML_Parser parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, &s);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, handleText);
  while ((len = read(0,buf,1024)) > 0) {
    if (!XML_Parse(parser, buf, len, (len<1024))) {
      return 1;
    }
  }
  XML_ParserFree(parser);
  iterate_ns(n.ns,print_node);
  printf("\n");
  return 0;
}
