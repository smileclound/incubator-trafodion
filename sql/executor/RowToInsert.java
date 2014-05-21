// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2013-2014 Hewlett-Packard Development Company, L.P.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// @@@ END COPYRIGHT @@@

package org.trafodion.sql.HBaseAccess;

import java.util.Vector;

public class RowToInsert extends Vector<RowToInsert.ColToInsert> {

	public class ColToInsert {
		public byte[] qualName;
		public byte[] colValue;
	}

	private static final long serialVersionUID = 5066470006717527862L;

	public void addColumn(byte[] name, byte[] value) {
		ColToInsert col = new ColToInsert();
		col.qualName = name;
		col.colValue = value;
		add(col);
	}

}


