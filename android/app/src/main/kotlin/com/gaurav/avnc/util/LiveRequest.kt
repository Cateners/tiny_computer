/*
 * Copyright (c) 2024  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.launch
import java.util.concurrent.LinkedBlockingQueue

/**
 * Extension of [LiveEvent] to facilitate awaiting some response from observers.
 *
 * This simplifies the cases where a background thread needs some value from the user (i.e. UI thread),
 * and we want the background thread to block until that value is available.
 *
 * If a request is canceled then [requestResponse] will return [cancellationValue].
 *
 * @param scope can be specified to auto-cancel this request on scope cancellation.
 */
class LiveRequest<RequestType, ResponseType>(private val cancellationValue: ResponseType, scope: CoroutineScope?)
    : LiveEvent<RequestType>() {

    private val responses = LinkedBlockingQueue<ResponseType>()

    init {
        scope?.launch { awaitCancellation() }?.invokeOnCompletion { cancelRequest() }
    }

    /**
     * Fires this request with given value and returns the response.
     * Will block until any response is available.
     * Can be called from any threads.
     */
    fun requestResponse(value: RequestType): ResponseType {
        responses.clear()
        fireAsync(value)
        return responses.take() //Blocking call
    }

    /**
     * Sets response for current request.
     */
    fun offerResponse(response: ResponseType) = responses.offer(response)

    /**
     * Cancels any pending request.
     */
    fun cancelRequest() = responses.offer(cancellationValue)
}