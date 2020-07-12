// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018-2020 SCANOSS LTD
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include "utils/base.h"
#include "scan_direct.h"

void scan_direct_scan_request_handler(api_request *req)
{
  char *filename = extract_qs_value(req->form, "file", MAX_PATH);
  char *assets = extract_qs_value(req->form, "assets", MAX_PATH);
  char *scantype = extract_qs_value(req->form, "type", MAX_SCAN_CODE);
  if (filename == NULL)
  {
    log_debug("No file supplied returning bad request");
    bad_request(req);
    if (filename != NULL)
    {
      free(filename);
    }
    return;
  }

  // validate scan type
  if (scantype && !strstr("ignore,identify,blacklist", scantype))
  {
    log_warn("Invalid scan type: %s", scantype);
    error_t *error = calloc(1, sizeof(error_t));
    strcpy(error->code, "INVALID");
    strcpy(error->message, "Invalid scan type");
    bad_request_with_error(req, error);
    Free_all(filename, scantype, error);
    if (assets)
      free(assets);
  }
  free(filename);
  char *tmpfile = extract_qs_value(req->form, "tmpfile", SCAN_FILE_MAX_SIZE);
  if (!tmpfile || tmpfile[0] == 0)
  {
    log_debug("Unable to find tmpfile, returning bad request");
    bad_request(req);
    return;
  }
  char tmpfilepath[MAX_PATH];
  sprintf(tmpfilepath, "%s/%s", FILE_DOWNLOAD_TMP_DIR, tmpfile);

  scan_direct_scan(req, tmpfilepath, assets, scantype);



free(tmpfile);
if (scantype)
  free(scantype);
}

/**
 * scan_direct_scan: Scans a wfp file and returns the result. Optionally, it takes a project identifier. If project_id value is 0, it assumes no project.
 * It returns output or NULL if there was a problem with the scanner
 */
void scan_direct_scan(api_request *req, char *path, char *assets, char *scantype)
{
  char command[512];
  if (assets == NULL)
  {
    sprintf(command, SCANOSS_CMD_SCAN_DIRECT_PLAIN_TMPL, path);
  }
  else
  {
    // Save assets to temp file.
    char assets_filename[256];
    char md5[64];
    md5sum(md5, assets, strlen(assets));
    sprintf(assets_filename, "/tmp/%s.json", md5);
    write_file(assets_filename, assets, strlen(assets));
    sprintf(command, SCANOSS_CMD_SCAN_DIRECT_TMPL, scantype, assets_filename, path);
  }

  log_debug("Executing %s\n", command);
  FILE *fp = popen(command, "r");
  if (fp == NULL)
  {
    log_error("Scan Execution failed for direct scan: %s", path);
    pclose(fp);
    return;
  }

  char buf[SCAN_DIRECT_LINE_SIZE];

  // Send HTTP Headers
  fgets(buf, sizeof(buf) - 1, fp);
  if (buf == NULL || buf[0] == 'E')
  {
    log_warn("Scanner returned exit status: %s", buf);
    send_http_status(req, 500, buf);
    pclose(fp);
    return;
  }

  int len = return_json_headers(req, 200);
  send_empty_line(req);
  len += 2;
  // Very important to print what we have already read from the file.
  http_print_str(req, buf);
  len += strlen(buf);
  // Send the rest until the end.
  len += send_stream(req, fp);
  pclose(fp);
  req->response_length = len;
  log_access(req, 200);
  log_debug("Finished scanning %s", path);
}
