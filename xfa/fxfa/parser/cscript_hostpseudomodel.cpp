// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cscript_hostpseudomodel.h"

#include <memory>

#include "fxjs/xfa/cjx_hostpseudomodel.h"

CScript_HostPseudoModel::CScript_HostPseudoModel(CXFA_Document* pDocument)
    : CXFA_Object(pDocument,
                  XFA_ObjectType::Object,
                  XFA_Element::HostPseudoModel,
                  std::make_unique<CJX_HostPseudoModel>(this)) {}

CScript_HostPseudoModel::~CScript_HostPseudoModel() = default;
