#!/usr/bin/python

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017 Osimis, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Affero General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.



# We do not use Python's "email" package, as it uses LF (\n) for line
# endings instead of CRLF (\r\n) for binary messages, as required by
# RFC 1341
# http://stackoverflow.com/questions/3086860/how-do-i-generate-a-multipart-mime-message-with-correct-crlf-in-python
# https://www.w3.org/Protocols/rfc1341/7_2_Multipart.html

import requests
import sys
import json
import uuid

#if len(sys.argv) < 2:
if len(sys.argv) < 1:
    print('Usage: %s <StowUri> <file>...' % sys.argv[0])
    print('')
    print('Example: %s http://localhost:8042/dicom-web/studies hello.dcm world.dcm' % sys.argv[0])
    sys.exit(-1)

URL = sys.argv[1]

# Create a multipart message whose body contains all the input DICOM files
boundary = str(uuid.uuid4())  # The boundary is a random UUID
body = bytearray()

for i in range(2, len(sys.argv)):
    try:
        with open(sys.argv[i], 'rb') as f:
            content = f.read()
            body += bytearray('--%s\r\n' % boundary, 'ascii')
            body += bytearray('Content-Length: %d\r\n' % len(content), 'ascii')
            body += bytearray('Content-Type: application/dicom\r\n\r\n', 'ascii')
            body += content
            body += bytearray('\r\n', 'ascii')
    except:
        print('Ignoring directory %s' % sys.argv[i])

# Closing boundary
body += bytearray('--%s--' % boundary, 'ascii')

# Do the HTTP POST request to the STOW-RS server
r = requests.post(URL, data=body, headers= {
    'Content-Type' : 'multipart/related; type=application/dicom; boundary=%s' % boundary,
    'Accept' : 'application/json',
})

j = json.loads(r.text)

# Loop over the successful instances
print('\nWADO-RS URL of the uploaded instances:')
for instance in j['00081199']['Value']:
    if '00081190' in instance:  # This instance has not been discarded
        url = instance['00081190']['Value'][0]
        print(url)

print('\nWADO-RS URL of the study:')
try:
    print(j['00081190']['Value'][0])
except:
    print('No instance was uploaded!')
