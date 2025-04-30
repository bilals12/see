const DEBUG = true;  // Set to false to disable debug logging

function debugLog(message, data) {
    if (DEBUG) {
        console.log(`[DEBUG] ${message}`, data ? data : '');
    }
}

// define colors for each event
const eventColors = {
    leftclicks: '#90EE90',    // light green
    rightclicks: '#98FB98',   // pale green
    middleclicks: '#98FF98',  // mint green
    keypresses: '#7FFF00',    // chartreuse
    mousemoves: '#00FF7F'     // spring green
};

// cumulative data display
fetch('./cumulative_data.csv')
    .then(response => response.text())
    .then(data => {
        debugLog('Cumulative data:', data);
        const lines = data.split('\n');
        if (lines.length > 1) {
            const [keypresses, mousemoves, leftclicks, rightclicks] = lines[1].split(',');
            
            document.getElementById('keypresses').textContent = keypresses;
            document.getElementById('mousemoves').textContent = mousemoves + 'm';
            document.getElementById('leftclicks').textContent = leftclicks;
            document.getElementById('rightclicks').textContent = rightclicks;
        }
    })
    .catch(error => console.error('error loading cumulative data:', error));

// fetch past 24h data + generate chart
fetch('./past_24_hours_data.csv')
    .then(response => response.text())
    .then(data => {
        const parsedData = parseCSV(data);
        renderChart(parsedData);
    })
    .catch(error => console.error('error loading past 24 hours data:', error));

function parseCSV(data) {
    debugLog('Raw CSV data:', data);
    const lines = data.split('\n')
        .filter(line => line.trim() && !line.startsWith('cumulative')); // skip header and cumulative line
    
    debugLog('Filtered lines:', lines);
    
    const parsedData = lines.map(line => {
        const [timestamp, keypresses, mousemoves, leftclicks, rightclicks, middleclicks] = line.split(',');
        const parsed = {
            timestamp: new Date(parseInt(timestamp) * 1000),
            keypresses: parseInt(keypresses) || 0,
            mousemoves: parseFloat(mousemoves.replace(' meters', '')) || 0,
            leftclicks: parseInt(leftclicks) || 0,
            rightclicks: parseInt(rightclicks) || 0,
            middleclicks: parseInt(middleclicks) || 0
        };
        debugLog('Parsed line:', parsed);
        return parsed;
    }).filter(d => !isNaN(d.timestamp.getTime()));

    debugLog('Final parsed data:', parsedData);
    return parsedData;
}

function renderChart(data) {
    if (!data || data.length === 0) {
        debugLog('No data to render');
        return;
    }
    debugLog('Rendering chart with data points:', data.length);
    
    // clear existing chart and tooltips
    d3.select('#chart').html('');
    d3.selectAll('.tooltip').remove();

    // setup dimensions
    const chartContainer = d3.select('#chart');
    const containerWidth = parseInt(chartContainer.style('width'));
    const containerHeight = parseInt(chartContainer.style('height'));
    const margin = {top: 20, right: 20, bottom: 30, left: 50};
    const width = (containerWidth || 800) - margin.left - margin.right;
    const height = (containerHeight || 400) - margin.top - margin.bottom;

    // create SVG
    const svg = chartContainer
        .append('svg')
        .style('width', '100%')
        .style('height', '100%')
        .attr('viewBox', `0 0 ${width + margin.left + margin.right} ${height + margin.top + margin.bottom}`)
        .append('g')
        .attr('transform', `translate(${margin.left},${margin.top})`);

    // setup scales
    const x = d3.scaleTime()
        .domain(d3.extent(data, d => d.timestamp))
        .range([0, width]);

    const y = d3.scaleLinear()
        .domain([0, d3.max(data, d => 
            Math.max(d.keypresses, d.mousemoves, d.leftclicks, d.rightclicks)
        ) * 1.1])
        .range([height, 0]);

    // create tooltip once
    const tooltip = d3.select('body')
        .append('div')
        .attr('class', 'tooltip')
        .style('opacity', 0);

    // draw grid
    function createGrid() {
        svg.append('g')
            .attr('class', 'grid')
            .attr('transform', `translate(0,${height})`)
            .call(d3.axisBottom(x)
                .ticks(d3.timeMinute.every(5))
                .tickSize(-height)
                .tickFormat(''))
            .style('stroke', '#0f0')
            .style('stroke-opacity', 0.05);

        svg.append('g')
            .attr('class', 'grid')
            .call(d3.axisLeft(y)
                .tickSize(-width)
                .tickFormat(''))
            .style('stroke', '#0f0')
            .style('stroke-opacity', 0.1);
    }

    // draw axes
    function createAxes() {
        // dynamic tick number
        const tickCount = Math.max(4, Math.min(12, Math.floor(width / 120)));
        const xAxis = d3.axisBottom(x)
            .tickFormat(d => {
                const hours = d.getHours();
                const minutes = d.getMinutes();
                const ampm = hours >= 12 ? 'PM' : 'AM';
                const hour12 = hours % 12 || 12;
                // only show minutes if not 0 or multiple of 15
                if (minutes === 0) {
                    return `${hour12}${ampm}`;
                } else if (data.length > 20 && minutes % 30 !== 0) {
                    // dense data: only show hour and .5 hour marks
                    return '';
                } else {
                    return `${hour12}:${minutes.toString().padStart(2, '0')}`;
                }
            })
            .ticks(tickCount)
            .tickSizeOuter(0);

        svg.append('g')
            .attr('transform', `translate(0,${height})`)
            .call(xAxis)
            .style('color', '#0f0')
            .style('font-family', 'monospace')
            .style('font-size', '12px');

        svg.append('g')
            .attr('class', 'y-axis')
            .call(d3.axisLeft(y).ticks(5))
            .style('color', '#0f0');
    }

    // draw data lines and points
    function drawDataLines() {
        const filteredEventColors = Object.fromEntries(
            Object.entries(eventColors).filter(([key]) => key !== 'middleclicks')
        );

        Object.entries(filteredEventColors).forEach(([metric, color]) => {
            // draw line
            const line = d3.line()
                .x(d => x(d.timestamp))
                .y(d => y(d[metric]))
                .defined(d => !isNaN(d[metric]));

            svg.append('path')
                .datum(data)
                .attr('fill', 'none')
                .attr('stroke', color)
                .attr('stroke-width', 2)
                .attr('d', line);

            // draw points
            if (data.length < 100) {
                svg.selectAll(`dot-${metric}`)
                    .data(data)
                    .enter()
                    .append('circle')
                    .attr('cx', d => x(d.timestamp))
                    .attr('cy', d => y(d[metric]))
                    .attr('r', 3)
                    .attr('fill', color)
                    .attr('opacity', 0.7);
            }
        });
    }

    // create mouse tracking overlay
    function createMouseTracking() {
        const overlay = svg.append('rect')
            .attr('class', 'overlay')
            .attr('width', width)
            .attr('height', height)
            .attr('fill', 'none')
            .style('pointer-events', 'all');

        const verticalLine = svg.append('line')
            .attr('class', 'tracking-line')
            .attr('y1', 0)
            .attr('y2', height)
            .style('stroke', '#0f0')
            .style('stroke-width', '1px')
            .style('opacity', 0);

        overlay.on('mousemove', function(event) {
            const [mouseX] = d3.pointer(event);
            const xDate = x.invert(mouseX);
            const bisect = d3.bisector(d => d.timestamp).left;
            const i = bisect(data, xDate, 1);
            // edge case
            if (i <= 0 || i >= data.length) return;
            const d0 = data[i - 1];
            const d1 = data[i];
            const d = xDate - d0.timestamp > d1.timestamp - xDate ? d1 : d0;

            verticalLine
                .attr('x1', x(d.timestamp))
                .attr('x2', x(d.timestamp))
                .style('opacity', 0.5);

            tooltip.style('opacity', 0.9)
                .html(`
                    <div>time: ${d.timestamp.toLocaleTimeString()}</div>
                    <div>keypresses: ${d.keypresses}</div>
                    <div>mouse moves: ${d.mousemoves.toFixed(2)}m</div>
                    <div>left clicks: ${d.leftclicks}</div>
                    <div>right clicks: ${d.rightclicks}</div>
                `)
                .style('left', `${event.pageX + 15}px`)
                .style('top', `${event.pageY - 10}px`);
        });

        overlay.on('mouseout', () => {
            tooltip.style('opacity', 0);
            verticalLine.style('opacity', 0);
        });
    }

    // initialize chart components
    createGrid();
    createAxes();
    drawDataLines();
    createMouseTracking();

    // auto-refresh stats
    setInterval(() => {
        fetch('./cumulative_data.csv')
            .then(response => response.text())
            .then(data => {
                const lines = data.split('\n');
                if (lines.length > 1) {
                    const [keypresses, mousemoves, leftclicks, rightclicks] = lines[1].split(',');
                    document.getElementById('keypresses').textContent = keypresses;
                    document.getElementById('mousemoves').textContent = mousemoves + 'm';
                    document.getElementById('leftclicks').textContent = leftclicks;
                    document.getElementById('rightclicks').textContent = rightclicks;
                }
            });
    }, 60000);
}

