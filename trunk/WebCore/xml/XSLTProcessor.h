/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
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
 *
 */

#ifndef XSLTProcessor_h
#define XSLTProcessor_h

#if ENABLE(XSLT)

#include "StringHash.h"
#include "XSLStyleSheet.h"
#include <wtf/HashMap.h>
#ifndef __OWB__
#include <libxml/parserInternals.h>
#include <libxslt/documents.h>
#else
#include "BTProcessor.h"
#endif

namespace WebCore {

class Frame;
class Node;
class Document;
class DocumentFragment;

class XSLTProcessor : public Shared<XSLTProcessor>
{
#ifdef __OWB__
friend class BAL::BTProcessor;
#endif
public:
    void setXSLStylesheet(XSLStyleSheet *styleSheet) { m_stylesheet = styleSheet; }
    bool transformToString(Node *source, DeprecatedString &resultMIMEType, DeprecatedString &resultString, DeprecatedString &resultEncoding);
    RefPtr<Document> createDocumentFromSource(const DeprecatedString& source, const DeprecatedString& sourceEncoding, const DeprecatedString& sourceMIMEType, Node* sourceNode, Frame* frame);
    
    // DOM methods
    void importStylesheet(Node *style) { m_stylesheetRootNode = style; }
    RefPtr<DocumentFragment> transformToFragment(Node *source, Document *ouputDoc);
    RefPtr<Document> transformToDocument(Node *source);
    
    void setParameter(const String& namespaceURI, const String& localName, const String& value);
    String getParameter(const String& namespaceURI, const String& localName) const;
    void removeParameter(const String& namespaceURI, const String& localName);
    void clearParameters() { m_parameters.clear(); }

    void reset() { m_stylesheet = NULL; m_stylesheetRootNode = NULL;  m_parameters.clear(); }
    
public:
    // Only for libXSLT callbacks
    XSLStyleSheet *xslStylesheet() const { return m_stylesheet.get(); }
    
    typedef HashMap<String, String> ParameterMap;

private:
    // Convert a libxml doc ptr to a KHTML DOM Document
#ifndef __OWB__
    RefPtr<Document> documentFromXMLDocPtr(xmlDocPtr resultDoc, xsltStylesheetPtr sheet, Document *ownerDocument, bool sourceIsDocument);
#endif

    RefPtr<XSLStyleSheet> m_stylesheet;
    RefPtr<Node> m_stylesheetRootNode;

    ParameterMap m_parameters;
};

}

#endif
#endif
