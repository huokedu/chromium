// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/common/print_messages.h"
#include "content/common/view_messages.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/metafile_skia_wrapper.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "ui/gfx/point.h"

#if !defined(OS_CHROMEOS)
#include "base/process_util.h"
#endif  // !defined(OS_CHROMEOS)

using WebKit::WebFrame;
using WebKit::WebNode;

void PrintWebViewHelper::RenderPreviewPage(int page_number) {
  PrintMsg_PrintPage_Params page_params;
  page_params.params = print_preview_context_.print_params();
  page_params.page_number = page_number;

  base::TimeTicks begin_time = base::TimeTicks::Now();
  PrintPageInternal(page_params,
                    print_preview_context_.GetPrintCanvasSize(),
                    print_preview_context_.frame(),
                    print_preview_context_.metafile());
  print_preview_context_.RenderedPreviewPage(
      base::TimeTicks::Now() - begin_time);
  PreviewPageRendered(page_number);
}

bool PrintWebViewHelper::PrintPages(const PrintMsg_PrintPages_Params& params,
                                    WebFrame* frame,
                                    WebNode* node) {
  printing::NativeMetafile metafile;
  if (!metafile.Init())
    return false;

  int page_count = 0;
  if (!RenderPages(params, frame, node, &page_count, &metafile))
    return false;

  // Get the size of the resulting metafile.
  uint32 buf_size = metafile.GetDataSize();
  DCHECK_GT(buf_size, 0u);

#if defined(OS_CHROMEOS)
  int sequence_number = -1;
  base::FileDescriptor fd;

  // Ask the browser to open a file for us.
  Send(new PrintHostMsg_AllocateTempFileForPrinting(&fd, &sequence_number));
  if (!metafile.SaveToFD(fd))
    return false;

  // Tell the browser we've finished writing the file.
  Send(new PrintHostMsg_TempFileForPrintingWritten(sequence_number));
  return true;
#else
  PrintHostMsg_DidPrintPage_Params printed_page_params;
  printed_page_params.data_size = 0;
  printed_page_params.document_cookie = params.params.document_cookie;

  base::SharedMemoryHandle shared_mem_handle;
  Send(new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size,
                                                  &shared_mem_handle));
  if (!base::SharedMemory::IsHandleValid(shared_mem_handle)) {
    NOTREACHED() << "AllocateSharedMemoryBuffer returned bad handle";
    return false;
  }

  {
    base::SharedMemory shared_buf(shared_mem_handle, false);
    if (!shared_buf.Map(buf_size)) {
      NOTREACHED() << "Map failed";
      return false;
    }
    metafile.GetData(shared_buf.memory(), buf_size);
    printed_page_params.data_size = buf_size;
    shared_buf.GiveToProcess(base::GetCurrentProcessHandle(),
                             &(printed_page_params.metafile_data_handle));
  }

  if (params.pages.empty()) {
    // Send the first page with a valid handle.
    printed_page_params.page_number = 0;
    Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));

    // Send the rest of the pages with an invalid metafile handle.
    printed_page_params.metafile_data_handle.fd = -1;
    for (int i = 1; i < page_count; ++i) {
      printed_page_params.page_number = i;
      Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));
    }
  } else {
    // Send the first page with a valid handle.
    printed_page_params.page_number = params.pages[0];
    Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));

    // Send the rest of the pages with an invalid metafile handle.
    printed_page_params.metafile_data_handle.fd = -1;
    for (size_t i = 1; i < params.pages.size(); ++i) {
      printed_page_params.page_number = params.pages[i];
      Send(new PrintHostMsg_DidPrintPage(routing_id(), printed_page_params));
    }
  }
  return true;
#endif  // defined(OS_CHROMEOS)
}

bool PrintWebViewHelper::RenderPages(const PrintMsg_PrintPages_Params& params,
                                     WebKit::WebFrame* frame,
                                     WebKit::WebNode* node,
                                     int* page_count,
                                     printing::Metafile* metafile) {
  PrintMsg_Print_Params printParams = params.params;

  UpdatePrintableSizeInPrintParameters(frame, node, &printParams);

  PrepareFrameAndViewForPrint prep_frame_view(printParams, frame, node);
  *page_count = prep_frame_view.GetExpectedPageCount();
  if (!*page_count)
    return false;

#if !defined(OS_CHROMEOS)
    Send(new PrintHostMsg_DidGetPrintedPagesCount(routing_id(),
                                                  printParams.document_cookie,
                                                  *page_count));
#endif

  base::TimeTicks begin_time = base::TimeTicks::Now();
  base::TimeTicks page_begin_time = begin_time;

  PrintMsg_PrintPage_Params page_params;
  page_params.params = printParams;
  const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
  if (params.pages.empty()) {
    for (int i = 0; i < *page_count; ++i) {
      page_params.page_number = i;
      PrintPageInternal(page_params, canvas_size, frame, metafile);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      page_params.page_number = params.pages[i];
      PrintPageInternal(page_params, canvas_size, frame, metafile);
    }
  }

  base::TimeDelta render_time = base::TimeTicks::Now() - begin_time;

  prep_frame_view.FinishPrinting();
  metafile->FinishDocument();
  return true;
}

void PrintWebViewHelper::PrintPageInternal(
    const PrintMsg_PrintPage_Params& params,
    const gfx::Size& canvas_size,
    WebFrame* frame,
    printing::Metafile* metafile) {
  double content_width_in_points;
  double content_height_in_points;
  double margin_top_in_points;
  double margin_right_in_points;
  double margin_bottom_in_points;
  double margin_left_in_points;
  GetPageSizeAndMarginsInPoints(frame,
                                params.page_number,
                                params.params,
                                &content_width_in_points,
                                &content_height_in_points,
                                &margin_top_in_points,
                                &margin_right_in_points,
                                &margin_bottom_in_points,
                                &margin_left_in_points);

  gfx::Size page_size(
      content_width_in_points + margin_right_in_points +
          margin_left_in_points,
      content_height_in_points + margin_top_in_points +
          margin_bottom_in_points);
  gfx::Rect content_area(margin_left_in_points, margin_top_in_points,
                         content_width_in_points, content_height_in_points);

  SkDevice* device = metafile->StartPageForVectorCanvas(
      params.page_number, page_size, content_area, 1.0f);
  if (!device)
    return;

  // The printPage method take a reference to the canvas we pass down, so it
  // can't be a stack object.
  SkRefPtr<skia::VectorCanvas> canvas = new skia::VectorCanvas(device);
  canvas->unref();  // SkRefPtr and new both took a reference.
  printing::MetafileSkiaWrapper::SetMetafileOnCanvas(canvas.get(), metafile);
  frame->printPage(params.page_number, canvas.get());

  // TODO(myhuang): We should render the header and the footer.

  // Done printing. Close the device context to retrieve the compiled metafile.
  if (!metafile->FinishPage())
    NOTREACHED() << "metafile failed";
}
