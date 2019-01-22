// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_WSDLCONNECTION_H_
#define FXJS_XFA_CJX_WSDLCONNECTION_H_

#include "fxjs/xfa/cjx_node.h"
#include "fxjs/xfa/jse_define.h"

class CXFA_WsdlConnection;

class CJX_WsdlConnection final : public CJX_Node {
 public:
  explicit CJX_WsdlConnection(CXFA_WsdlConnection* connection);
  ~CJX_WsdlConnection() override;

  JSE_METHOD(execute);

 private:
  using Type__ = CJX_WsdlConnection;

  static const CJX_MethodSpec MethodSpecs[];
};

#endif  // FXJS_XFA_CJX_WSDLCONNECTION_H_
