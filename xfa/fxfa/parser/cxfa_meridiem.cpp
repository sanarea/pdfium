// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_meridiem.h"

#include <memory>

#include "fxjs/xfa/cjx_node.h"

CXFA_Meridiem::CXFA_Meridiem(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_LocaleSet,
                XFA_ObjectType::ContentNode,
                XFA_Element::Meridiem,
                {},
                {},
                std::make_unique<CJX_Node>(this)) {}

CXFA_Meridiem::~CXFA_Meridiem() = default;
