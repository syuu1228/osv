#
# Copyright (C) 2017 Takuya ASADA
#
# This work is open source software, licensed under the terms of the
# BSD license as described in the LICENSE file in the top-level directory.
#

from osv.modules.api import *
from osv.modules.filemap import FileMap
from osv.modules import api

usr_files = FileMap()
usr_files.add('${OSV_BASE}/modules/vmm').to('/') \
	.include('vmm.so')

default = api.run('/vmm.so')
