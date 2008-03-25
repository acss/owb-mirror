/**
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "XSLStyleSheet.h"

#if ENABLE(XSLT)

#include "CString.h"
#include "DocLoader.h"
#include "Document.h"
#include "Node.h"
#include "XSLImportRule.h"
#include "loader.h"
#include "XMLTokenizer.h"
#ifndef __OWB__
#include <libxml/uri.h>
#include <libxslt/xsltutils.h>
#endif
#if PLATFORM(MAC)
#include "SoftLinking.h"
#endif

#ifdef __OWB__
#include "ParseXML.h"
#endif //__OWB__

#if PLATFORM(MAC)
SOFT_LINK_LIBRARY(libxslt)
SOFT_LINK(libxslt, xsltIsBlank, int, (xmlChar *str), (str))
SOFT_LINK(libxslt, xsltGetNsProp, xmlChar *, (xmlNodePtr node, const xmlChar *name, const xmlChar *nameSpace), (node, name, nameSpace))
SOFT_LINK(libxslt, xsltParseStylesheetDoc, xsltStylesheetPtr, (xmlDocPtr doc), (doc))
SOFT_LINK(libxslt, xsltLoadStylesheetPI, xsltStylesheetPtr, (xmlDocPtr doc), (doc))
#endif

namespace WebCore {

XSLStyleSheet::XSLStyleSheet(XSLImportRule* parentRule, const String& href)
    : StyleSheet(parentRule, href)
    , m_ownerDocument(0)
#ifndef __OWB__
    , m_stylesheetDoc(0)
    , m_embedded(false)
#else
    , m_styleSheet(new BAL::BTStyleSheet(this, false))
#endif
    , m_processed(false) // Child sheets get marked as processed when the libxslt engine has finally seen them.
#ifndef __OWB__
    , m_stylesheetDocTaken(false)
#endif
{
}

XSLStyleSheet::XSLStyleSheet(Node* parentNode, const String& href,  bool embedded)
    : StyleSheet(parentNode, href)
    , m_ownerDocument(parentNode->document())
#ifndef __OWB__
    , m_stylesheetDoc(0)
    , m_embedded(embedded)
#else
    , m_styleSheet(new BAL::BTStyleSheet(this, embedded))
#endif
    , m_processed(true) // The root sheet starts off processed.
#ifndef __OWB__
    , m_stylesheetDocTaken(false)
#endif
{
}

XSLStyleSheet::~XSLStyleSheet()
{
#ifndef __OWB__
    if (!m_stylesheetDocTaken)
        xmlFreeDoc(m_stylesheetDoc);
#endif
}

bool XSLStyleSheet::isLoading()
{
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        StyleBase* rule = item(i);
        if (rule->isImportRule()) {
            XSLImportRule* import = static_cast<XSLImportRule*>(rule);
            if (import->isLoading())
                return true;
        }
    }
    return false;
}

void XSLStyleSheet::checkLoaded()
{
    if (isLoading()) 
        return;
    if (parent())
        parent()->checkLoaded();
    if (m_parentNode)
        m_parentNode->sheetLoaded();
}

#ifndef __OWB__
xmlDocPtr XSLStyleSheet::document()
{
    if (m_embedded && ownerDocument())
        return (xmlDocPtr)ownerDocument()->transformSource();
    return m_stylesheetDoc;
}
#endif

void XSLStyleSheet::clearDocuments()
{
#ifndef __OWB__
    m_stylesheetDoc = 0;
#else
    m_styleSheet->clearDocuments();
#endif
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        StyleBase* rule = item(i);
        if (rule->isImportRule()) {
            XSLImportRule* import = static_cast<XSLImportRule*>(rule);
            if (import->styleSheet())
                import->styleSheet()->clearDocuments();
        }
    }
}

DocLoader* XSLStyleSheet::docLoader()
{
    if (!m_ownerDocument)
        return 0;
    return m_ownerDocument->docLoader();
}

bool XSLStyleSheet::parseString(const String& string, bool strict)
{
    // Parse in a single chunk into an xmlDocPtr
#ifdef __OWB__
    const UChar BOM = 0xFEFF;
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&BOM);
    BAL::setLoaderForLibXMLCallbacks(docLoader());
#else
    setLoaderForLibXMLCallbacks(docLoader());
#endif

#ifndef __OWB__
    if (!m_stylesheetDocTaken)
        xmlFreeDoc(m_stylesheetDoc);
    m_stylesheetDocTaken = false;
    m_stylesheetDoc = xmlReadMemory(reinterpret_cast<const char*>(string.characters()), string.length() * sizeof(UChar),
        m_ownerDocument->URL().ascii(),
        BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
        XML_PARSE_NOENT | XML_PARSE_DTDATTR | XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOCDATA);
#else
    bool result = m_styleSheet->parseString(string, strict);
#endif

    loadChildSheets();

#ifdef __OWB__
    BAL::setLoaderForLibXMLCallbacks(0);
#else
    setLoaderForLibXMLCallbacks(0);
#endif

#ifndef __OWB__
    return m_stylesheetDoc;
#else
    return result;
#endif
}

void XSLStyleSheet::loadChildSheets()
{
#ifndef __OWB__
    if (!document())
        return;
    
    xmlNodePtr stylesheetRoot = document()->children;
    
    // Top level children may include other things such as DTD nodes, we ignore those.
    while (stylesheetRoot && stylesheetRoot->type != XML_ELEMENT_NODE)
        stylesheetRoot = stylesheetRoot->next;
    
    if (m_embedded) {
        // We have to locate (by ID) the appropriate embedded stylesheet element, so that we can walk the 
        // import/include list.
        xmlAttrPtr idNode = xmlGetID(document(), (const xmlChar*)(href().utf8().data()));
        if (!idNode)
            return;
        stylesheetRoot = idNode->parent;
    } else {
        // FIXME: Need to handle an external URI with a # in it.  This is a pretty minor edge case, so we'll deal
        // with it later.
    }
    
    if (stylesheetRoot) {
        // Walk the children of the root element and look for import/include elements.
        // Imports must occur first.
        xmlNodePtr curr = stylesheetRoot->children;
        while (curr) {
            if (curr->type != XML_ELEMENT_NODE) {
                curr = curr->next;
                continue;
            }
            if (IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "import")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);                
                loadChildSheet(DeprecatedString::fromUtf8((const char*)uriRef));
                xmlFree(uriRef);
            } else
                break;
            curr = curr->next;
        }

        // Now handle includes.
        while (curr) {
            if (curr->type == XML_ELEMENT_NODE && IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "include")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);
                loadChildSheet(DeprecatedString::fromUtf8((const char*)uriRef));
                xmlFree(uriRef);
            }
            curr = curr->next;
        }
    }
#else
    m_styleSheet->loadChildSheets();
#endif
}

void XSLStyleSheet::loadChildSheet(const DeprecatedString& href)
{
    RefPtr<XSLImportRule> childRule = new XSLImportRule(this, href);
    append(childRule);
    childRule->loadSheet();
}

#ifndef __OWB__
xsltStylesheetPtr XSLStyleSheet::compileStyleSheet()
{
    // FIXME: Hook up error reporting for the stylesheet compilation process.
    if (m_embedded)
        return xsltLoadStylesheetPI(document());
    
    // xsltParseStylesheetDoc makes the document part of the stylesheet
    // so we have to release our pointer to it.
    ASSERT(!m_stylesheetDocTaken);
    xsltStylesheetPtr result = xsltParseStylesheetDoc(m_stylesheetDoc);
    if (result)
        m_stylesheetDocTaken = true;
    return result;
}

xmlDocPtr XSLStyleSheet::locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri)
{
    bool matchedParent = (parentDoc == document());
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        StyleBase* rule = item(i);
        if (rule->isImportRule()) {
            XSLImportRule* import = static_cast<XSLImportRule*>(rule);
            XSLStyleSheet* child = import->styleSheet();
            if (!child)
                continue;
            if (matchedParent) {
                if (child->processed())
                    continue; // libxslt has been given this sheet already.
                
                // Check the URI of the child stylesheet against the doc URI.
                // In order to ensure that libxml canonicalized both URLs, we get the original href
                // string from the import rule and canonicalize it using libxml before comparing it
                // with the URI argument.
                CString importHref = import->href().utf8();
                xmlChar* base = xmlNodeGetBase(parentDoc, (xmlNodePtr)parentDoc);
                xmlChar* childURI = xmlBuildURI((const xmlChar*)importHref.data(), base);
                bool equalURIs = xmlStrEqual(uri, childURI);
                xmlFree(base);
                xmlFree(childURI);
                if (equalURIs) {
                    child->markAsProcessed();
                    return child->document();
                }
            } else {
                xmlDocPtr result = import->styleSheet()->locateStylesheetSubResource(parentDoc, uri);
                if (result)
                    return result;
            }
        }
    }
    
    return 0;
}
#endif

void XSLStyleSheet::markAsProcessed()
{
    ASSERT(!m_processed);
#ifndef __OWB__
    ASSERT(!m_stylesheetDocTaken);
#else
    m_styleSheet->markAsProcessed();
#endif
    m_processed = true;
#ifndef __OWB__
    m_stylesheetDocTaken = true;
#endif
}

} // namespace WebCore

#endif // ENABLE(XSLT)
