/*
This file is part of Fire-IE.

Fire-IE is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fire-IE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fire-IE.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @fileOverview Provides content policy delegation
 */

var EXPORTED_SYMBOLS = ["ContentPolicyDelegate"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

let baseURL = Cc["@fireie.org/fireie/private;1"].getService(Ci.nsIURI);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import(baseURL.spec + "Utils.jsm");

let contentPolicyDelegates = {};

/**
 * Retrieves the delegate used for content policy evaluation
 */
let ContentPolicyDelegate = {
  getContentPolicyDelegate: function(delegateType)
  {
    return contentPolicyDelegates[delegateType];
  }
};

contentPolicyDelegates = {
  "ABP": {
    invoke: function(contentType, contentLocation, requestOrigin, node)
    {
      // stub implementation, filters a predefined set of stuff
      let result = contentLocation != "http://www.baidu.com/img/baidu_sylogo1.gif"
        && contentLocation != "http://www.baidu.com/img/baidu_jgylogo3.gif"
        && contentLocation != "http://img.baidu.com/img/post-jg.gif"
        && contentLocation.indexOf("http://tb2.bdstatic.com/tb/static-common/img/tieba_logo") != 0
        && contentLocation.indexOf("http://360.cn") != 0
        && contentLocation.indexOf("http://static.youku.com/v1.0.0223/v/swf") != 0
      ;
      if (result) return Ci.nsIContentPolicy.ACCEPT;
      else return Ci.nsIContentPolicy.REJECT_OTHER;
    }
  }
};

