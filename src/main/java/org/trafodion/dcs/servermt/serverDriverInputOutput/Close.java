/**
 *(C) Copyright 2015 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.trafodion.dcs.servermt.serverDriverInputOutput; 

import java.util.*;
import java.nio.*;
import java.io.*;
import java.math.*;

import org.trafodion.dcs.Constants;
import org.trafodion.dcs.util.*;
import org.trafodion.dcs.servermt.ServerConstants;
import org.trafodion.dcs.servermt.ServerUtils;
import org.trafodion.dcs.servermt.serverHandler.*;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
    
public class Close {
    private static  final Log LOG = LogFactory.getLog(Close.class);
    private int dialogueId;
    private String stmtLabel;
    private short freeResourceOpt;

    public Close(){
        dialogueId = 0;
        stmtLabel = "";
        freeResourceOpt = 0;
    }
    public void extractFromByteBuffer(ByteBuffer bbBuf) throws UnsupportedEncodingException {
        try {
            dialogueId = bbBuf.getInt();
            stmtLabel=ByteBufferUtils.extractString(bbBuf);
            freeResourceOpt = bbBuf.getShort();
            debugConnectionContext("extract");
        } catch(UnsupportedEncodingException ue){
            LOG.error("Close :UnsupportedEncodingException :" + ue);
            throw new UnsupportedEncodingException(ue.getMessage());
        }
    }
    public void debugConnectionContext(String function){
        if(LOG.isDebugEnabled()){
            LOG.debug("Function :" + function);
            LOG.debug("dialogueId :"+dialogueId);
            LOG.debug("stmtLabel :"+stmtLabel);
            LOG.debug("freeResourceOpt :"+freeResourceOpt);
        }
    }
}
